#ifndef _MV_TASK_H_
#define _MV_TASK_H_

#include <list>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>

#include "Utility/require.hpp"
#include "Utility/signal.hpp"
#include "Utility/exactType.hpp"

namespace MV {

	class Task;

	class ActionBase {
		friend Task;
	public:
		virtual std::string name() const {
			return "Base";
		}

		virtual void onStart(Task& a_self) {}

		virtual void onFinish(Task& a_self) {}
		virtual void onFinishAll(Task& a_self) {}

		virtual void onSuspend(Task& a_self) {}
		virtual void onResume(Task& a_self) {}

		virtual void onCancel(Task& a_self) {}
		virtual void onCancelUpdate(Task& a_self) {}

		virtual void onException(Task& a_self, std::exception& e) {}

		//Return true on finish.
		virtual bool update(Task& a_self, double a_dt) {
			return true;
		}

	protected:
		virtual bool handleExceptions() {
			return false;
		}

		void initialize(Task* a_ourTask);
	};

	class BasicAction : public ActionBase {
	public:
		virtual std::string name() const override {
			return "BasicAction";
		}

		BasicAction(bool a_infinite = false) : infinite(a_infinite) {}

		virtual bool update(Task& a_self, double a_dt) override { return !infinite; }

	private:

		bool infinite;
	};

	class Task {
	private:
		Signal<void(Task&)> onStartSignal;
		Signal<void(Task&)> onFinishSignal;
		Signal<void(Task&)> onFinishAllSignal;
		Signal<void(Task&)> onSuspendSignal;
		Signal<void(Task&)> onResumeSignal;
		Signal<void(Task&)> onCancelSignal;
		Signal<void(Task&)> onCancelUpdateSignal;

		Signal<void(Task&, std::exception &)> onExceptionSignal;

	public:
		Task(ExactType<bool> a_infinite = false, ExactType<bool> a_blocking = true, ExactType<bool> a_blockParentCompletion = true) :
			Task("root", a_infinite, a_blocking, a_blockParentCompletion) {
		}

		Task(const std::string &a_name, ExactType<bool> a_infinite = false, ExactType<bool> a_blocking = true, ExactType<bool> a_blockParentCompletion = true) :
			Task(a_name, std::make_shared<BasicAction>(a_infinite), a_blocking, a_blockParentCompletion) {
			if (a_infinite) {
				unblockChildren();
			}
		}

		Task(const std::string &a_name, std::function<bool(Task&, double)> a_task, ExactType<bool> a_blocking = true, ExactType<bool> a_blockParentCompletion = true) :
			Task(a_name, Receiver<bool(Task&, double)>::make(a_task), a_blocking, a_blockParentCompletion) {
		}

		Task(const std::string &a_name, const Receiver<bool(Task&, double)>::SharedType &a_task, ExactType<bool> a_blocking = true, ExactType<bool> a_blockParentCompletion = true) :
			taskName(a_name),
			task(a_task),
			block(a_blocking),
			blockParentCompletion(a_blockParentCompletion),
			onStart(onStartSignal),
			onFinishAll(onFinishAllSignal),
			onFinish(onFinishSignal),
			onCancel(onCancelSignal),
			onCancelUpdate(onCancelUpdateSignal),
			onSuspend(onSuspendSignal),
			onResume(onResumeSignal),
			onException(onExceptionSignal) {
		}

		Task(const std::string &a_name, const std::shared_ptr<ActionBase> &a_action, ExactType<bool> a_blocking = true, ExactType<bool> a_blockParentCompletion = true) :
			Task(a_name, std::function<bool(Task&, double)>([](Task& a_self, double a_dt) {return a_self.optionalAction->update(a_self, a_dt); }), a_blocking, a_blockParentCompletion) {
			registerActionBase(a_action);
		}

		Task(const std::shared_ptr<ActionBase> &a_action, ExactType<bool> a_blocking = true, ExactType<bool> a_blockParentCompletion = true) :
			Task(a_action->name(), [](Task& a_self, double a_dt) {return a_self.optionalAction->update(a_self, a_dt); }, a_blocking, a_blockParentCompletion) {
			registerActionBase(a_action);
		}

		Task(const Task&) = delete;
		Task(Task&&) = delete;
		Task operator=(const Task&) = delete;

		~Task() {
			if (!cancelled) {
				if (!ourTaskComplete) {
					onCancelUpdateSignal(*this);
				}
				onCancelSignal(*this);
			}
		}

		Task* recent() {
			return (!mostRecentCreated.expired()) ? mostRecentCreated.lock().get() : nullptr;
		}

		Task& localInterval(double a_dt, size_t a_maxUpdates = 0) {
			localDeltaInterval = a_dt;
			maxUpdates = a_maxUpdates;
			return *this;
		}

		Task& interval(double a_dt, size_t a_maxUpdates = 0) {
			deltaInterval = a_dt;
			maxUpdates = a_maxUpdates;
			return *this;
		}

		std::string name() const {
			return taskName;
		}

		bool update(double a_dt) {
			if (finished()) { return true; }

			unsuspend();

			totalTime += a_dt;
			if (deltaInterval > 0) {
				totalUpdateIntervals();
			}
			else {
				lastCalledInterval = totalTime;
				totalUpdateStep(a_dt);
			}
			return finished();
		}

		bool finished() {
			return cancelled || (ourTaskComplete && noChildrenBlockingCompletion());
		}

		double localElapsed() const {
			return lastCalledLocalInterval;
		}

		double elapsed() const {
			return lastCalledInterval;
		}

		Task& unblockChildren() {
			alwaysRunChildren = true;
			return *this;
		}

		Task& blockChildren() {
			alwaysRunChildren = false;
			return *this;
		}

		Task& cancel() {
			cancelAllChildren();
			if (!cancelled) {
				cancelled = true;
				if (ourTaskStarted) {
					if (!ourTaskComplete) {
						ourTaskComplete = true;
						try { onCancelUpdateSignal(*this); }
						catch (std::exception &a_e) {
							handleCallbackException(a_e);
						}
					}

					try { onCancelSignal(*this); }
					catch (std::exception &a_e) {
						handleCallbackException(a_e);
					}
				}
			}
			return *this;
		}

		bool blocking() const {
			return block;
		}

		Task& now(const std::shared_ptr<Task> &a_other) {
			if (!sequentialTasks.empty()) {
				sequentialTasks.front()->suspend();
			}
			childrenBlockingOurCompletionCount += static_cast<int>(a_other->blockParentCompletion);
			sequentialTasks.push_front(a_other);
			mostRecentCreated = a_other;
			resetFinishState();
			return *this;
		}

		Task& now(const std::string &a_name, const Receiver<bool(Task&, double)>::SharedType &a_task, ExactType<bool> a_blockParentCompletion = true) {
			return now(std::make_shared<Task>(a_name, a_task, true, a_blockParentCompletion));
		}

		Task& now(const std::string &a_name, std::function<bool(Task&, double)> a_task, ExactType<bool> a_blockParentCompletion = true) {
			return now(std::make_shared<Task>(a_name, a_task, true, a_blockParentCompletion));
		}

		Task& now(const std::string &a_name, ExactType<bool> a_blockParentCompletion = true) {
			return now(a_name, std::make_shared<BasicAction>(false), a_blockParentCompletion);
		}

		Task& now(const std::string &a_name, const std::shared_ptr<ActionBase> &a_action, ExactType<bool> a_blockParentCompletion = true) {
			now(a_name, [](Task& a_self, double a_dt) {return a_self.optionalAction->update(a_self, a_dt); }, a_blockParentCompletion);
			recent()->registerActionBase(a_action);
			return *this;
		}

		Task& now(const std::shared_ptr<ActionBase> &a_action, ExactType<bool> a_blockParentCompletion = true) {
			return now(a_action->name(), a_action, a_blockParentCompletion);
		}

		Task& then(const std::shared_ptr<Task> &a_other) {
			childrenBlockingOurCompletionCount += static_cast<int>(a_other->blockParentCompletion);
			sequentialTasks.push_back(a_other);
			mostRecentCreated = a_other;
			resetFinishState();
			return *this;
		}

		Task& then(const std::string &a_name, const Receiver<bool(Task&, double)>::SharedType &a_task, ExactType<bool> a_blockParentCompletion = true) {
			return then(std::make_shared<Task>(a_name, a_task, true, a_blockParentCompletion));
		}

		Task& then(const std::string &a_name, std::function<bool(Task&, double)> a_task, ExactType<bool> a_blockParentCompletion = true) {
			return then(std::make_shared<Task>(a_name, a_task, true, a_blockParentCompletion));
		}

		Task& then(const std::string &a_name, ExactType<bool> a_blockParentCompletion = true) {
			return then(a_name, std::make_shared<BasicAction>(false), a_blockParentCompletion);
		}

		Task& then(const std::string &a_name, const std::shared_ptr<ActionBase> &a_action, ExactType<bool> a_blockParentCompletion = true) {
			then(a_name, [](Task& a_self, double a_dt) {return a_self.optionalAction->update(a_self, a_dt); }, a_blockParentCompletion);
			recent()->registerActionBase(a_action);
			return *this;
		}

		Task& then(const std::shared_ptr<ActionBase> &a_action, ExactType<bool> a_blockParentCompletion = true) {
			return then(a_action->name(), a_action, a_blockParentCompletion);
		}

		Task& thenAlso(const std::shared_ptr<Task> &a_other) {
			childrenBlockingOurCompletionCount += static_cast<int>(a_other->blockParentCompletion);
			sequentialTasks.push_back(a_other);
			a_other->block = false;
			mostRecentCreated = a_other;
			resetFinishState();
			return *this;
		}

		Task& thenAlso(const std::string &a_name, const Receiver<bool(Task&, double)>::SharedType &a_task, ExactType<bool> a_blockParentCompletion = true) {
			return thenAlso(std::make_shared<Task>(a_name, a_task, false, a_blockParentCompletion));
		}

		Task& thenAlso(const std::string &a_name, std::function<bool(Task&, double)> a_task, ExactType<bool> a_blockParentCompletion = true) {
			return thenAlso(std::make_shared<Task>(a_name, a_task, false, a_blockParentCompletion));
		}

		Task& thenAlso(const std::string &a_name, ExactType<bool> a_infinite = false, ExactType<bool> a_blockParentCompletion = true) {
			thenAlso(a_name, std::make_shared<BasicAction>(a_infinite), a_blockParentCompletion);
			if (a_infinite) {
				recent()->unblockChildren();
			}
			return *this;
		}

		Task& thenAlso(const std::string &a_name, const std::shared_ptr<ActionBase> &a_action, ExactType<bool> a_blockParentCompletion = true) {
			thenAlso(a_name, [](Task& a_self, double a_dt) {return a_self.optionalAction->update(a_self, a_dt); }, a_blockParentCompletion);
			recent()->registerActionBase(a_action);
			return *this;
		}

		Task& thenAlso(const std::shared_ptr<ActionBase> &a_action, ExactType<bool> a_blockParentCompletion = true) {
			return thenAlso(a_action->name(), a_action, a_blockParentCompletion);
		}

		Task& also(const std::shared_ptr<Task> &a_other) {
			childrenBlockingOurCompletionCount += static_cast<int>(a_other->blockParentCompletion);
			parallelTasks.push_back(a_other);
			resetFinishState();
			mostRecentCreated = parallelTasks.back();
			return *this;
		}

		Task& also(const std::string &a_name, const Receiver<bool(Task&, double)>::SharedType &a_task, ExactType<bool> a_blockParentCompletion = true) {
			return also(std::make_shared<Task>(a_name, a_task, false, a_blockParentCompletion));
		}

		Task& also(const std::string &a_name, std::function<bool(Task&, double)> a_task, ExactType<bool> a_blockParentCompletion = true) {
			return also(std::make_shared<Task>(a_name, a_task, false, a_blockParentCompletion));
		}

		Task& also(const std::string &a_name, ExactType<bool> a_infinite = false, ExactType<bool> a_blockParentCompletion = true) {
			also(a_name, std::make_shared<BasicAction>(a_infinite), a_blockParentCompletion);
			if (a_infinite) {
				recent()->unblockChildren();
			}
			return *this;
		}

		Task& also(const std::string &a_name, const std::shared_ptr<ActionBase> &a_action, ExactType<bool> a_blockParentCompletion = true) {
			also(a_name, [](Task& a_self, double a_dt) {return a_self.optionalAction->update(a_self, a_dt); }, a_blockParentCompletion);
			recent()->registerActionBase(a_action);
			return *this;
		}

		Task& also(const std::shared_ptr<ActionBase> &a_action, ExactType<bool> a_blockParentCompletion = true) {
			return also(a_action->name(), a_action, a_blockParentCompletion);
		}

		bool sequenceContains(const std::string &a_reference) const {
			return std::find_if(sequentialTasks.begin(), sequentialTasks.end(), [&](const std::shared_ptr<Task> &a_item) {return a_item->name() == a_reference; }) != sequentialTasks.end();
		}

		Task& after(const std::string &a_reference, const std::shared_ptr<Task> &a_other) {
			auto found = std::find_if(sequentialTasks.begin(), sequentialTasks.end(), [&](const std::shared_ptr<Task> &a_item) {return a_item->name() == a_reference; });
			require<ResourceException>(found != sequentialTasks.end(), "Task::after missing [", a_reference, "]");
			++found;
			childrenBlockingOurCompletionCount += static_cast<int>(a_other->blockParentCompletion);
			sequentialTasks.insert(found, a_other);
			mostRecentCreated = a_other;
			resetFinishState();
			return *this;
		}

		Task& after(const std::string &a_reference, const std::string &a_name, const Receiver<bool(Task&, double)>::SharedType &a_task, ExactType<bool> a_blockParentCompletion = true) {
			return after(a_reference, std::make_shared<Task>(a_name, a_task, true, a_blockParentCompletion));
		}

		Task& after(const std::string &a_reference, const std::string &a_name, std::function<bool(Task&, double)> a_task, ExactType<bool> a_blockParentCompletion = true) {
			return after(a_reference, std::make_shared<Task>(a_name, a_task, true, a_blockParentCompletion));
		}

		Task& after(const std::string &a_reference, const std::string &a_name, ExactType<bool> a_blockParentCompletion = true) {
			return after(a_reference, a_name, std::make_shared<BasicAction>(false), a_blockParentCompletion);
		}

		Task& after(const std::string &a_reference, const std::string &a_name, const std::shared_ptr<ActionBase> &a_action, ExactType<bool> a_blockParentCompletion = true) {
			after(a_reference, a_name, [](Task& a_self, double a_dt) {return a_self.optionalAction->update(a_self, a_dt); }, a_blockParentCompletion);
			recent()->registerActionBase(a_action);
			return *this;
		}

		Task& after(const std::string &a_reference, std::shared_ptr<ActionBase> &a_action, ExactType<bool> a_blockParentCompletion = true) {
			return after(a_reference, a_action->name(), a_action, a_blockParentCompletion);
		}

		Task& before(const std::string &a_reference, const std::shared_ptr<Task> &a_other) {
			auto found = std::find_if(sequentialTasks.begin(), sequentialTasks.end(), [&](const std::shared_ptr<Task> &a_item) {return a_item->name() == a_reference; });
			require<ResourceException>(found != sequentialTasks.end(), "Task::before missing [", a_reference, "]");
			if (found == sequentialTasks.begin()) {
				(*found)->suspend();
			}
			childrenBlockingOurCompletionCount += static_cast<int>(a_other->blockParentCompletion);
			sequentialTasks.insert(found, a_other);
			mostRecentCreated = a_other;
			resetFinishState();
			return *this;
		}

		Task& before(const std::string &a_reference, const std::string &a_name, const Receiver<bool(Task&, double)>::SharedType &a_task, ExactType<bool> a_blockParentCompletion = true) {
			return before(a_reference, std::make_shared<Task>(a_name, a_task, true, a_blockParentCompletion));
		}

		Task& before(const std::string &a_reference, const std::string &a_name, std::function<bool(Task&, double)> a_task, ExactType<bool> a_blockParentCompletion = true) {
			return before(a_reference, std::make_shared<Task>(a_name, a_task, true, a_blockParentCompletion));
		}

		Task& before(const std::string &a_reference, const std::string &a_name, ExactType<bool> a_blockParentCompletion = true) {
			return before(a_reference, a_name, std::make_shared<BasicAction>(false), a_blockParentCompletion);
		}

		Task& before(const std::string &a_reference, const std::string &a_name, const std::shared_ptr<ActionBase> &a_action, ExactType<bool> a_blockParentCompletion = true) {
			before(a_reference, a_name, [](Task& a_self, double a_dt) {return a_self.optionalAction->update(a_self, a_dt); }, a_blockParentCompletion);
			recent()->registerActionBase(a_action);
			return *this;
		}

		Task& before(const std::string &a_reference, std::shared_ptr<ActionBase> &a_action, ExactType<bool> a_blockParentCompletion = true) {
			return before(a_reference, a_action->name(), a_action, a_blockParentCompletion);
		}

		Task* get(const std::string &a_name, bool a_throwOnNotFound = true) {
			auto foundInSequentials = std::find_if(sequentialTasks.begin(), sequentialTasks.end(), [&](const std::shared_ptr<Task> &a_task) {
				return a_task->taskName == a_name;
			});
			if (foundInSequentials != sequentialTasks.end()) {
				return (*foundInSequentials).get();
			}

			auto foundInParallels = std::find_if(parallelTasks.begin(), parallelTasks.end(), [&](const std::shared_ptr<Task> &a_task) {
				return a_task->taskName == a_name;
			});
			if (foundInParallels != parallelTasks.end()) {
				return (*foundInParallels).get();
			}

			require<ResourceException>(!a_throwOnNotFound, "Failed to find: [", a_name, "] in task: [", taskName, "]");
			return nullptr;
		}

		Task* getDeep(const std::string &a_name, bool a_throwOnNotFound = true) {
			Task* found = get(a_name, false);
			if (found) {
				return found;
			}

			for (auto&& task : sequentialTasks) {
				Task* found = task->getDeep(a_name, false);
				if (found) {
					return found;
				}
			}

			for (auto&& task : parallelTasks) {
				Task* found = task->getDeep(a_name, false);
				if (found) {
					return found;
				}
			}

			require<ResourceException>(!a_throwOnNotFound, "Failed to find: [", a_name, "] in task: [", taskName, "]");
			return nullptr;
		}

		SignalRegister<void(Task&)> onStart;
		SignalRegister<void(Task&)> onFinish;
		SignalRegister<void(Task&)> onFinishAll;
		SignalRegister<void(Task&)> onSuspend;
		SignalRegister<void(Task&)> onResume;
		SignalRegister<void(Task&)> onCancel;
		SignalRegister<void(Task&)> onCancelUpdate;
		SignalRegister<void(Task&, std::exception &)> onException;

	private:
		bool cancelled = false;

		void handleCallbackException(std::exception &a_e) {
			std::cerr << "Task [" << name() << "]:" << a_e.what() << std::endl;
			if (onExceptionSignal.cullDeadObservers() == 0) { throw; }
			onExceptionSignal(*this, a_e);
		}

		void registerActionBase(const std::shared_ptr<ActionBase> &a_base) {
			a_base->initialize(this);
			optionalAction = a_base;
		}

		void startIfNeeded() {
			if (!ourTaskStarted) {
				ourTaskStarted = true;

				try { onStartSignal(*this); }
				catch (std::exception &a_e) {
					handleCallbackException(a_e);
				}
			}
		}

		void resetFinishState() {
			if (ourTaskComplete) {
				totalLocalTime = 0.0f;
			}
			onFinishAllSignal.unblock();
			onFinishSignal.unblock();
			cancelled = false;
		}

		void totalUpdateIntervals() {
			if (currentStep == 0 && !suspended && !finished()) {
				++currentStep;
				totalUpdateStep(0.0);
			}

			size_t steps = static_cast<size_t>((totalTime - lastCalledInterval) / deltaInterval);
			for (size_t i = 0; i < steps && !suspended && !finished() && (maxUpdates == 0 || i < maxUpdates); ++i) {
				++currentStep;
				lastCalledInterval += deltaInterval;
				totalUpdateStep(deltaInterval);
			}
		}

		void totalUpdateStep(double a_dt) {
			try {
				if (!cancelled) {
					startIfNeeded();

					updateLocalTask(a_dt);
					updateParallelTasks(a_dt);
					if ((ourTaskComplete || alwaysRunChildren) && updateChildTasks(a_dt)) {
						onFinishAllSignal(*this);
						onFinishAllSignal.block();
					}
				}
			}
			catch (std::exception &a_e) {
				handleCallbackException(a_e);
			}
		}

		void localTaskUpdateIntervals() {
			if (currentLocalStep == 0 && !suspended && !finished()) {
				++currentLocalStep;
				localTaskUpdateStep(0.0f);
			}

			size_t steps = static_cast<size_t>((totalLocalTime - lastCalledLocalInterval) / localDeltaInterval);
			for (size_t i = 0; i < steps && !suspended && !cancelled && !ourTaskComplete && (maxUpdates == 0 || i < maxUpdates); ++i) {
				++currentLocalStep;
				lastCalledLocalInterval += localDeltaInterval;
				localTaskUpdateStep(localDeltaInterval);
			}
		}

		void updateLocalTask(double a_dt) {
			if (!ourTaskComplete) {
				totalLocalTime += a_dt;

				if (localDeltaInterval > 0) {
					localTaskUpdateIntervals();
				}
				else {
					lastCalledLocalInterval = totalLocalTime;
					localTaskUpdateStep(a_dt);
				}
			}
		}

		void localTaskUpdateStep(double a_dt) {
			if (!ourTaskComplete) {
				try {
					if (task->predicate(*this, a_dt)) {
						ourTaskComplete = true;
						try { onFinishSignal(*this); }
						catch (std::exception &a_e) {
							handleCallbackException(a_e);
						}
						onFinishSignal.block();
					}
				}
				catch (std::exception &a_e) {
					handleCallbackException(a_e);
				}
			}
		}

		void updateSequentialTasks(double a_dt) {
			while (true) {
				auto firstBlockingTask = std::find_if(sequentialTasks.begin(), sequentialTasks.end(), [](const std::shared_ptr<Task> &a_task) {
					return a_task->block;
				});
				if (firstBlockingTask != sequentialTasks.begin()) {
					parallelTasks.insert(parallelTasks.end(), make_move_iterator(sequentialTasks.begin()), make_move_iterator(firstBlockingTask));
					sequentialTasks.erase(sequentialTasks.begin(), firstBlockingTask);
				}
				if (!sequentialTasks.empty()) {
					auto currentSequentialTask = sequentialTasks.begin();
					if ((*currentSequentialTask)->update(a_dt)) {
						childrenBlockingOurCompletionCount -= static_cast<int>((*currentSequentialTask)->blockParentCompletion);
						sequentialTasks.erase(currentSequentialTask);
						a_dt = 0;
						continue; //Avoid forcing a frame between sequential task completions if they immediately finish.
					}
				}
				break;
			}
		}

		void updateParallelTasks(double a_dt) {
			auto task = parallelTasks.begin();
			auto end = parallelTasks.end();
			while (task != end) {
				bool needToErase = false;
				try {
					needToErase = (*task)->update(a_dt);
				}
				catch (std::exception &a_e) {
					handleCallbackException(a_e);
					needToErase = true;
				}
				if (needToErase) {
					childrenBlockingOurCompletionCount -= static_cast<int>((*task)->blockParentCompletion);
					parallelTasks.erase(task++);
				}
				else {
					++task;
				}
			}
		}

		bool noChildrenBlockingCompletion() const {
			return childrenBlockingOurCompletionCount == 0;
		}

		void cancelAllChildren() {
			auto sequentialTasksToCancel = sequentialTasks;
			sequentialTasks.clear();
			for (auto&& task = sequentialTasksToCancel.rbegin(); task != sequentialTasksToCancel.rend(); ++task) {
				(*task)->cancel();
			}
			auto parallelTasksToCancel = parallelTasks;
			parallelTasks.clear();
			for (auto&& task = parallelTasksToCancel.rbegin(); task != parallelTasksToCancel.rend(); ++task) {
				(*task)->cancel();
			}
			childrenBlockingOurCompletionCount = 0;
		}

		void finishOurTaskAndChildren() {
			bool allFinished = finished();
			if (!ourTaskComplete) {
				if (!ourTaskStarted) {
					cancel();
				}
				else {
					ourTaskComplete = true;
					try { onFinishSignal(*this); }
					catch (std::exception &a_e) {
						handleCallbackException(a_e);
					}
					onFinishSignal.block();
				}
			}
			finishAllChildTasks();
			if (!allFinished && finished()) {
				try { onFinishAllSignal(*this); }
				catch (std::exception &a_e) {
					handleCallbackException(a_e);
				}
				onFinishAllSignal.block();
			}
		}

		void finishAllChildTasks() {
			auto parallelTasksToFinish = parallelTasks;
			parallelTasks.clear();
			for (std::shared_ptr<Task> &task : parallelTasksToFinish) {
				task->finishOurTaskAndChildren();
			}
			auto sequentialTasksToFinish = sequentialTasks;
			sequentialTasks.clear();
			childrenBlockingOurCompletionCount = 0;
			for (std::shared_ptr<Task> &task : sequentialTasksToFinish) {
				task->finishOurTaskAndChildren();
			}
		}

		bool cleanupChildTasks() {
			if (noChildrenBlockingCompletion()) {
				finishAllChildTasks();
				if (noChildrenBlockingCompletion()) {
					return true;
				}
			}
			return false;
		}

		bool updateChildTasks(double a_dt) {
			updateSequentialTasks(a_dt);

			return cleanupChildTasks();
		}

		void unsuspend() {
			if (suspended) {
				suspended = false;
				try { onResumeSignal(*this); }
				catch (std::exception &a_e) {
					handleCallbackException(a_e);
				}
			}
		}

		void suspend() {
			if (!suspended && ourTaskStarted) {
				suspended = true;
				try { onSuspendSignal(*this); }
				catch (std::exception &a_e) {
					handleCallbackException(a_e);
				}
			}
		}

		Receiver<bool(Task&, double)>::SharedType task;

		std::list<std::shared_ptr<Task>> parallelTasks;
		std::list<std::shared_ptr<Task>> sequentialTasks;

		std::string taskName;
		double totalTime = 0.0;
		double totalLocalTime = 0.0;
		double deltaInterval = 0.0;
		double localDeltaInterval = 0.0;
		double lastCalledInterval = 0.0;
		double lastCalledLocalInterval = 0.0;

		uint64_t currentStep = 0;
		uint64_t currentLocalStep = 0;

		size_t maxUpdates = 0;

		bool block;
		bool suspended = false;
		bool ourTaskComplete = false;
		bool ourTaskStarted = false;

		bool blockParentCompletion;
		bool alwaysRunChildren = false;

		int childrenBlockingOurCompletionCount = 0;

		std::weak_ptr<Task> mostRecentCreated;

		std::shared_ptr<ActionBase> optionalAction;
	};

	inline void ActionBase::initialize(Task* a_ourTask) {
		a_ourTask->onStart.connect("onStart", [&](Task& a_self) {onStart(a_self); });
		a_ourTask->onFinish.connect("onFinish", [&](Task& a_self) {onFinish(a_self); });
		a_ourTask->onFinishAll.connect("onFinishAll", [&](Task& a_self) {onFinishAll(a_self); });
		a_ourTask->onSuspend.connect("onSuspend", [&](Task& a_self) {onSuspend(a_self); });
		a_ourTask->onResume.connect("onResume", [&](Task& a_self) {onResume(a_self); });
		a_ourTask->onCancel.connect("onCancel", [&](Task& a_self) {onCancel(a_self); });
		a_ourTask->onCancelUpdate.connect("onCancelUpdate", [&](Task& a_self) {onCancelUpdate(a_self); });

		//If we register onException it will trap exceptions even if the ActionBase does nothing with it, so this needs to conditionally register.
		if (handleExceptions()) {
			a_ourTask->onException.connect("onException", [&](Task& a_self, std::exception &a_e) {onException(a_self, a_e); });
		}
	}

}
    
#endif
