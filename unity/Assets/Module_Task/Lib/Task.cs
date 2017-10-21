using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using UnityEngine;

namespace SharedCore
{
    /// <summary>
    /// Optional ActionBase, you don't *need* to extend from this to use the Task system, but if you want the structure, you can use it.
    /// </summary>
    public class ActionBase
    {
        public virtual string DefaultName { get { return this.GetType().Name; } }

        public virtual void OnStart(Task a_self)
        {
        }

        public virtual void OnFinish(Task a_self)
        {
        }

        public virtual void OnFinishAll(Task a_self)
        {
        }

        public virtual void OnSuspend(Task a_self)
        {
        }

        public virtual void OnResume(Task a_self)
        {
        }

        public virtual void OnCancel(Task a_self)
        {
        }

        public virtual void OnCancelUpdate(Task a_self)
        {
        }

        public virtual void OnException(Task a_self, Exception e)
        {
        }

        public virtual bool HandlesExceptions { get { return false; } }

        //Return true on finish.
        public virtual bool Update(Task a_self, double a_dt)
        {
            return true;
        }

        protected Task ourTask;
        public void register(Task a_ourTask)
        {
            ourTask = a_ourTask;
            ourTask.onStart += OnStart;
            ourTask.onFinish += OnFinish;
            ourTask.onFinishAll += OnFinishAll;
            ourTask.onSuspend += OnSuspend;
            ourTask.onResume += OnResume;
            ourTask.onCancel += OnCancel;
            ourTask.onCancelUpdate += OnCancelUpdate;

            //If we register onException it will trap exceptions even if the ActionBase does nothing with it, so this needs to conditionally register.
            if (HandlesExceptions)
                ourTask.onException += OnException;
        }
    }

    public delegate void TaskCallback(Task value);

    public delegate void TaskExceptionCallback(Task value, Exception e);

    public delegate bool TaskAction(Task task, double dt);

    //Note: Members are lower case because this is an adopted library
    public class Task
    {
        private Task ourParent = null;
        public Task parent()
        {
            return ourParent;
        }

        public Task root()
        {
            var current = this;
            while (current.ourParent != null)
            {
                current = current.ourParent;
            }
            return current;
        }

        private bool alwaysRunChildTasks;

        private bool block;
        private bool blockOnFinish;

        private bool blockOnFinishAll;

        private bool blockParentCompletion;
        private bool cancelled;
        private double deltaInterval;

        private double lastCalledInterval;
        private double lastCalledLocalInterval;

        private double localDeltaInterval;
        private bool ourTaskComplete = false;
        private bool ourTaskStarted = false;

        private int currentStep = 0;
        private int currentLocalStep = 0;

        public bool isCancelled()
        {
            return cancelled;
        }

        public bool localTaskStarted()
        {
            return ourTaskStarted;
        }

        public bool localTaskComplete()
        {
            return ourTaskComplete;
        }

        public bool alwaysRunChildren()
        {
            return alwaysRunChildTasks;
        }

        public List<Task> parallelTasks = new List<Task>();

        private Task recentlyCreated;
        public List<Task> sequentialTasks = new List<Task>();

        private bool suspended;

        private TaskAction task;

        private string taskName;
        private double totalLocalTime;

        private double totalTime;

        //Not all tasks have an action base, this is convenience for linking back to the extra state a task might have on it.
        private ActionBase optionalBase = null;
        public ActionBase OptionalBase { get { return optionalBase; } }

        public Task(bool a_infinite = false, bool a_blocking = true, bool a_blockParentCompletion = true)
        {
            initialize("root", delegate { return !a_infinite; }, a_blocking, a_blockParentCompletion);
            if (a_infinite)
            {
                unblockChildTasks();
            }
        }

        public Task(string a_name, bool a_infinite = false, bool a_blocking = true, bool a_blockParentCompletion = true)
        {
            initialize(a_name, delegate { return !a_infinite; }, a_blocking, a_blockParentCompletion);
            if (a_infinite)
            {
                unblockChildTasks();
            }
        }

        public Task(string a_name, TaskAction a_task, bool a_blocking = true, bool a_blockParentCompletion = true)
        {
            initialize(a_name, a_task, a_blocking, a_blockParentCompletion);
        }

        public Task(string a_name, ActionBase a_task, bool a_blocking = true, bool a_blockParentCompletion = true)
        {
            initialize(a_name, a_task.Update, a_blocking, a_blockParentCompletion);
            registerActionBase(a_task);
        }

        public Task(ActionBase a_task, bool a_blocking = true, bool a_blockParentCompletion = true)
        {
            initialize(a_task.DefaultName, a_task.Update, a_blocking, a_blockParentCompletion);
            registerActionBase(a_task);
        }

        //public:
        public event TaskCallback onStart;
        public event TaskCallback onFinishAll;
        public event TaskCallback onFinish;
        public event TaskCallback onSuspend;
        public event TaskCallback onResume;
        public event TaskCallback onCancel;
        public event TaskCallback onCancelUpdate;

        public event TaskExceptionCallback onException;

        public Task localInterval(double a_dt)
        {
            localDeltaInterval = a_dt;
            return this;
        }

        public Task interval(double a_dt)
        {
            deltaInterval = a_dt;
            return this;
        }

        public string name()
        {
            return taskName;
        }

        public bool update(double a_dt)
        {
            if (!finished())
            {
                unsuspend();

                totalTime += a_dt;
                if (deltaInterval > 0)
                {
                    totalUpdateIntervals();
                }
                else
                {
                    lastCalledInterval = totalTime;
                    totalUpdateStep(a_dt);
                }
            }
            return finished();
        }

        public bool finished()
        {
            return cancelled || (ourTaskComplete && noChildrenBlockingCompletion());
        }

        public double localElapsed()
        {
            return lastCalledLocalInterval;
        }

        public double elapsed()
        {
            return lastCalledInterval;
        }

        public Task unblockChildTasks()
        {
            alwaysRunChildTasks = true;
            return this;
        }

        public Task blockChildTasks()
        {
            alwaysRunChildTasks = false;
            return this;
        }

        public bool childTasksBlockedByLocal()
        {
            return !alwaysRunChildTasks;
        }

        public Task cancel()
        {
            cancelChildren();
            if (!cancelled)
            {
                cancelled = true;
                if (ourTaskStarted)
                {
                    if (!ourTaskComplete)
                    {
                        ourTaskComplete = true;
                        if (onCancelUpdate != null)
                        {
                            try
                            {
                                onCancelUpdate(this);
                            }
                            catch (Exception e)
                            {
                                if (onException != null)
                                {
                                    onException(this, e);
                                }
                                else
                                {
                                    throw;
                                }
                            }
                        }
                    }
                    if (onCancel != null)
                    {
                        try
                        {
                            onCancel(this);
                        }
                        catch (Exception e)
                        {
                            if (onException != null)
                            {
                                onException(this, e);
                            }
                            else
                            {
                                throw;
                            }
                        }
                    }
                }
            }
            return this;
        }

        public Task cancelChildren()
        {
            foreach (var task in Enumerable.Reverse(sequentialTasks))
            {
                task.cancel();
            }
            sequentialTasks.Clear();
            foreach (var task in Enumerable.Reverse(parallelTasks))
            {
                task.cancel();
            }
            parallelTasks.Clear();
            return this;
        }

        public bool blocking()
        {
            return block;
        }

        //good safety method for "after" or "before" if you aren't 100% sure a task is in sequence.
        public bool sequenceContains(string a_reference)
        {
            return sequentialTasks.FindIndex(t => t.name() == a_reference) >= 0;
        }

        public Task after(string a_reference, Task a_task)
        {
            var index = sequentialTasks.FindIndex(t => t.name() == a_reference);
            if (index == -1)
                throw new ArgumentOutOfRangeException("Task.after(...) could not find: " + a_reference);

            recentlyCreated = a_task;
            recentlyCreated.ourParent = this;
            if (index + 1 == sequentialTasks.Count)
            {
                sequentialTasks.Add(recentlyCreated);
            }
            else
            {
                sequentialTasks.Insert(index + 1, recentlyCreated);
            }
            return this;
        }

        public Task after(string a_reference, string a_name, TaskAction a_task, bool a_blockParentCompletion = true)
        {
            var index = sequentialTasks.FindIndex(t => t.name() == a_reference);
            if (index == -1)
                throw new ArgumentOutOfRangeException("Task.after(...) could not find: " + a_reference);

            recentlyCreated = new Task(a_name, a_task, true, a_blockParentCompletion);
            recentlyCreated.ourParent = this;
            if (index + 1 == sequentialTasks.Count)
            {
                sequentialTasks.Add(recentlyCreated);
            }
            else
            {
                sequentialTasks.Insert(index + 1, recentlyCreated);
            }
            resetFinishFlags();
            return this;
        }

        public Task after(string a_reference, string a_name, bool a_blockParentCompletion = true)
        {
            return after(a_reference, a_name, delegate { return true; }, a_blockParentCompletion);
        }

        public Task after(string a_reference, string a_name, ActionBase a_action, bool a_blockParentCompletion = true)
        {
            after(a_reference, a_name, a_action.Update, a_blockParentCompletion);
            recentlyCreated.registerActionBase(a_action);
            return this;
        }

        public Task after(string a_reference, ActionBase a_action, bool a_blockParentCompletion = true)
        {
            return after(a_reference, a_action.DefaultName, a_action, a_blockParentCompletion);
        }

        public Task before(string a_reference, Task a_task)
        {
            var index = sequentialTasks.FindIndex(t => t.name() == a_reference);
            if (index == -1)
                throw new ArgumentOutOfRangeException("Task.before(...) could not find: " + a_reference);

            if (sequentialTasks.Count > 0 && index == 0)
            {
                sequentialTasks[0].suspend();
            }

            recentlyCreated = a_task;
            recentlyCreated.ourParent = this;
            sequentialTasks.Insert(index, recentlyCreated);
            resetFinishFlags();
            return this;
        }

        public Task before(string a_reference, string a_name, TaskAction a_task, bool a_blockParentCompletion = true)
        {
            var index = sequentialTasks.FindIndex(t => t.name() == a_reference);
            if (index == -1)
                throw new ArgumentOutOfRangeException("Task.before(...) could not find: " + a_reference);

            if (sequentialTasks.Count > 0 && index == 0)
            {
                sequentialTasks[0].suspend();
            }

            recentlyCreated = new Task(a_name, a_task, true, a_blockParentCompletion);
            recentlyCreated.ourParent = this;
            sequentialTasks.Insert(index, recentlyCreated);
            resetFinishFlags();
            return this;
        }

        public Task before(string a_reference, string a_name, bool a_blockParentCompletion = true)
        {
            return before(a_reference, a_name, delegate { return true; }, a_blockParentCompletion);
        }

        public Task before(string a_reference, string a_name, ActionBase a_action, bool a_blockParentCompletion = true)
        {
            before(a_reference, a_name, a_action.Update, a_blockParentCompletion);
            recentlyCreated.registerActionBase(a_action);
            return this;
        }

        public Task before(string a_reference, ActionBase a_action, bool a_blockParentCompletion = true)
        {
            return before(a_reference, a_action.DefaultName, a_action, a_blockParentCompletion);
        }

        public Task now(Task a_task)
        {
            if (sequentialTasks.Count > 0)
            {
                sequentialTasks[0].suspend();
            }

            recentlyCreated = a_task;
            recentlyCreated.ourParent = this;
            sequentialTasks.Insert(0, recentlyCreated);
            resetFinishFlags();
            return this;
        }

        public Task now(string a_name, TaskAction a_task, bool a_blockParentCompletion = true)
        {
            if (sequentialTasks.Count > 0)
            {
                sequentialTasks[0].suspend();
            }

            recentlyCreated = new Task(a_name, a_task, true, a_blockParentCompletion);
            recentlyCreated.ourParent = this;
            sequentialTasks.Insert(0, recentlyCreated);
            resetFinishFlags();
            return this;
        }

        public Task now(string a_name, bool a_blockParentCompletion = true)
        {
            return now(a_name, delegate { return true; }, a_blockParentCompletion);
        }

        public Task now(string a_name, ActionBase a_action, bool a_blockParentCompletion = true)
        {
            now(a_name, a_action.Update, a_blockParentCompletion);
            recentlyCreated.registerActionBase(a_action);
            return this;
        }

        public Task now(ActionBase a_action, bool a_blockParentCompletion = true)
        {
            return now(a_action.DefaultName, a_action, a_blockParentCompletion);
        }

        public Task then(Task a_task)
        {
            recentlyCreated = a_task;
            recentlyCreated.ourParent = this;
            sequentialTasks.Add(recentlyCreated);
            resetFinishFlags();
            return this;
        }

        public Task then(string a_name, TaskAction a_task, bool a_blockParentCompletion = true)
        {
            recentlyCreated = new Task(a_name, a_task, true, a_blockParentCompletion);
            recentlyCreated.ourParent = this;
            sequentialTasks.Add(recentlyCreated);
            resetFinishFlags();
            return this;
        }

        public Task then(string a_name, bool a_blockParentCompletion = true)
        {
            return then(a_name, delegate { return true; }, a_blockParentCompletion);
        }

        public Task then(string a_name, ActionBase a_action, bool a_blockParentCompletion = true)
        {
            then(a_name, a_action.Update, a_blockParentCompletion);
            recentlyCreated.registerActionBase(a_action);
            return this;
        }

        public Task then(ActionBase a_action, bool a_blockParentCompletion = true)
        {
            return then(a_action.DefaultName, a_action, a_blockParentCompletion);
        }

        public Task thenAlso(Task a_task)
        {
            recentlyCreated = a_task;
            recentlyCreated.block = false;
            recentlyCreated.ourParent = this;
            sequentialTasks.Add(recentlyCreated);
            resetFinishFlags();
            return this;
        }

        public Task thenAlso(string a_name, TaskAction a_task, bool a_blockParentCompletion = true)
        {
            recentlyCreated = new Task(a_name, a_task, false, a_blockParentCompletion);
            recentlyCreated.ourParent = this;
            sequentialTasks.Add(recentlyCreated);
            resetFinishFlags();
            return this;
        }

        public Task thenAlso(string a_name, bool a_infinite = false, bool a_blockParentCompletion = true)
        {
            var result = thenAlso(a_name, delegate { return !a_infinite; }, a_blockParentCompletion);
            if (a_infinite)
            {
                result.recent().unblockChildTasks();
            }
            return result;
        }

        public Task thenAlso(string a_name, ActionBase a_action, bool a_blockParentCompletion = true)
        {
            thenAlso(a_name, a_action.Update, a_blockParentCompletion);
            recentlyCreated.registerActionBase(a_action);
            return this;
        }

        public Task thenAlso(ActionBase a_action, bool a_blockParentCompletion = true)
        {
            return thenAlso(a_action.DefaultName, a_action, a_blockParentCompletion);
        }

        public Task also(Task a_task)
        {
            recentlyCreated = a_task;
            recentlyCreated.block = false;
            recentlyCreated.ourParent = this;
            parallelTasks.Add(recentlyCreated);
            resetFinishFlags();
            return this;
        }

        public Task also(string a_name, TaskAction a_task, bool a_blockParentCompletion = true)
        {
            recentlyCreated = new Task(a_name, a_task, false, a_blockParentCompletion);
            recentlyCreated.ourParent = this;
            parallelTasks.Add(recentlyCreated);
            resetFinishFlags();
            return this;
        }

        public Task also(string a_name, bool a_infinite = false, bool a_blockParentCompletion = true)
        {
            var result = also(a_name, delegate { return !a_infinite; }, a_blockParentCompletion);
            if (a_infinite)
            {
                result.recent().unblockChildTasks();
            }
            return result;
        }

        public Task also(string a_name, ActionBase a_action, bool a_blockParentCompletion = true)
        {
            also(a_name, a_action.Update, a_blockParentCompletion);
            recentlyCreated.registerActionBase(a_action);
            return this;
        }

        public Task also(ActionBase a_action, bool a_blockParentCompletion = true)
        {
            return also(a_action.DefaultName, a_action, a_blockParentCompletion);
        }

        public Task recent()
        {
            return recentlyCreated;
        }

        public Task get(string a_name, bool a_throwOnNotFound = true)
        {
            var foundInSequentials = sequentialTasks.Find(a_task => a_task.taskName == a_name);
            if (foundInSequentials != null)
            {
                return foundInSequentials;
            }
            var foundInParallels = parallelTasks.Find(a_task => a_task.taskName == a_name);
            if (foundInParallels != null)
            {
                return foundInParallels;
            }
            if (a_throwOnNotFound)
            {
                throw new Exception("Failed to find: [" + a_name + "] in task: [" + taskName + "]");
            }
            return null;
        }

        public Task getDeep(string a_name, bool a_throwOnNotFound = true)
        {
            var foundResult = get(a_name, false);
            if (foundResult != null)
                return foundResult;

            for (int i = 0; i < sequentialTasks.Count && foundResult == null; ++i)
            {
                foundResult = sequentialTasks[i].getDeep(a_name, false);
                if (foundResult != null)
                    return foundResult;
            }

            for (int i = 0; i < parallelTasks.Count && foundResult == null; ++i)
            {
                foundResult = parallelTasks[i].getDeep(a_name, false);
                if (foundResult != null)
                    return foundResult;
            }

            if (a_throwOnNotFound)
            {
                throw new Exception("Failed to deep find: [" + a_name + "] in task: [" + taskName + "]");
            }
            return null;
        }

        public bool noChildrenBlockingCompletion()
        {
            return (parallelTasks.FindIndex(a_task => a_task != null && a_task.blockParentCompletion) == -1) &&
                   (sequentialTasks.FindIndex(a_task => a_task != null && a_task.blockParentCompletion) == -1);
        }

        public bool empty()
        {
            return parallelTasks.Count == 0 && sequentialTasks.Count == 0;
        }

        //private:
        private void initialize(string a_name, TaskAction a_task, bool a_blocking, bool a_blockParentCompletion)
        {
            taskName = a_name;
            task = a_task;
            block = a_blocking;
            blockParentCompletion = a_blockParentCompletion;
            totalLocalTime = 0.0;
            totalTime = 0.0;
            ourTaskStarted = false;
            ourTaskComplete = false;
        }

        private void registerActionBase(ActionBase a_base)
        {
            a_base.register(this);
            optionalBase = a_base;
        }

        private void unsuspend()
        {
            if (suspended)
            {
                suspended = false;
                if (onResume != null)
                {
                    try
                    {
                        onResume(this);
                    }
                    catch (Exception e)
                    {
                        if (onException != null)
                        {
                            onException(this, e);
                        }
                        else
                        {
                            throw;
                        }
                    }
                }
            }
        }

        private void suspend()
        {
            if (!suspended && ourTaskStarted)
            {
                suspended = true;
                if (onSuspend != null)
                {
                    try
                    {
                        onSuspend(this);
                    }
                    catch (Exception e)
                    {
                        if (onException != null)
                        {
                            onException(this, e);
                        }
                        else
                        {
                            throw;
                        }
                    }
                }
            }
        }

        private void totalUpdateStep(double a_dt)
        {
            try
            {
                if (!cancelled)
                {
                    startIfNeeded();

                    updateLocalTask(a_dt);
                    updateParallelTasks(a_dt);
                    if ((ourTaskComplete || alwaysRunChildTasks) && updateChildTasks(a_dt))
                    {
                        callOnFinishAllDelegate();
                    }
                }
            }
            catch (Exception e)
            {
                if (onException != null)
                {
                    onException(this, e);
                }
                else
                {
                    throw;
                }
            }
        }

        private void startIfNeeded()
        {
            if (!ourTaskStarted && onStart != null)
            {
                ourTaskStarted = true;
                try
                {
                    onStart(this);
                }
                catch (Exception e)
                {
                    if (onException != null)
                    {
                        onException(this, e);
                    }
                    else
                    {
                        throw;
                    }
                }
            }
        }

        private void totalUpdateIntervals()
        {
            if (currentStep == 0 && !suspended && !finished())
            {
                ++currentStep;
                totalUpdateStep(0.0);
            }

            var steps = (int)((totalTime - lastCalledInterval) / deltaInterval);
            for (var i = 0; i < steps && !suspended && !finished(); ++i)
            {
                ++currentStep;
                lastCalledInterval += deltaInterval;
                totalUpdateStep(deltaInterval);
            }
        }

        private void updateLocalTask(double a_dt)
        {
            if (!ourTaskComplete)
            {
                totalLocalTime += a_dt;

                if (localDeltaInterval > 0)
                {
                    localTaskUpdateIntervals();
                }
                else
                {
                    lastCalledLocalInterval = totalLocalTime;
                    localTaskUpdateStep(a_dt);
                }
            }
        }

        private void localTaskUpdateIntervals()
        {
            if (currentLocalStep == 0 && !suspended && !finished())
            {
                ++currentLocalStep;
                localTaskUpdateStep(0.0f);
            }

            var steps = (int)((totalLocalTime - lastCalledLocalInterval) / localDeltaInterval);
            for (var i = 0; i < steps && !suspended && !cancelled & !ourTaskComplete; ++i)
            {
                ++currentLocalStep;
                lastCalledLocalInterval += localDeltaInterval;
                localTaskUpdateStep(localDeltaInterval);
            }
        }

        private void localTaskUpdateStep(double a_dt)
        {
            if (tryToCompleteLocalTaskWithDelta(a_dt))
            {
                ourTaskComplete = true;
                callOnFinishDelegate();
            }
        }

        private bool tryToCompleteLocalTaskWithDelta(double a_dt)
        {
            try
            {
                return (task(this, a_dt) && !cancelled);
            }
            catch (Exception e)
            {
                if (onException != null)
                {
                    onException(this, e);
                }
                else
                {
                    throw;
                }
                return true;
            }
        }

        private void callOnFinishDelegate()
        {
            if (!blockOnFinish)
            {
                blockOnFinish = true;
                if (onFinish != null)
                {
                    try
                    {
                        onFinish(this);
                    }
                    catch (Exception e)
                    {
                        if (onException != null)
                        {
                            onException(this, e);
                        }
                        else
                        {
                            throw;
                        }
                    }
                }
            }
        }

        private void callOnFinishAllDelegate()
        {
            if (!blockOnFinishAll)
            {
                blockOnFinishAll = true;
                if (onFinishAll != null)
                {
                    try
                    {
                        onFinishAll(this);
                    }
                    catch (Exception e)
                    {
                        if (onException != null)
                        {
                            onException(this, e);
                        }
                        else
                        {
                            throw;
                        }
                    }
                }
            }
        }

        private void updateSequentialTasks(double a_dt)
        {
            while (true)
            {
                var firstBlockingTask = sequentialTasks.FindIndex(a_task => a_task.block);
                if (firstBlockingTask > 0)
                {
                    parallelTasks.AddRange(sequentialTasks.GetRange(0, firstBlockingTask));
                    sequentialTasks.RemoveRange(0, firstBlockingTask);
                }
                else if (firstBlockingTask == -1)
                {
                    parallelTasks.AddRange(sequentialTasks.GetRange(0, sequentialTasks.Count));
                    sequentialTasks.Clear();
                }
                if (sequentialTasks.Count > 0)
                {
                    var currentSequentialTask = sequentialTasks[0];
                    if (currentSequentialTask.update(a_dt))
                    {
                        sequentialTasks.Remove(currentSequentialTask);
                        a_dt = 0;
                        continue; //Avoid forcing a frame between sequential task completions if they immediately finish.
                    }
                }
                break;
            }
        }

        private void updateParallelTasks(double a_dt)
        {
            var localParallel = parallelTasks.ToList();
            localParallel.ForEach(a_task => a_task.update(a_dt));
            parallelTasks.RemoveAll(a_task => a_task.finished());
        }

        private void finishOurTaskAndChildren()
        {
            var allFinished = finished();
            if (!ourTaskComplete)
            {
                if (!ourTaskStarted)
                {
                    cancel();
                    return;
                }

                ourTaskComplete = true;
                callOnFinishDelegate();
            }
            finishAllChildTasks();
            if (!allFinished)
            {
                callOnFinishAllDelegate();
            }
        }

        private void finishAllChildTasks()
        {
            foreach (var task in parallelTasks)
            {
                task.finishOurTaskAndChildren();
            }
            parallelTasks.Clear();
            foreach (var task in sequentialTasks)
            {
                task.finishOurTaskAndChildren();
            }
            sequentialTasks.Clear();
        }

        private bool cleanupChildTasks()
        {
            if (noChildrenBlockingCompletion())
            {
                finishAllChildTasks();
                return true;
            }
            return false;
        }

        private bool updateChildTasks(double a_dt)
        {
            updateSequentialTasks(a_dt);

            return cleanupChildTasks();
        }

        private void resetFinishFlags()
        {
            blockOnFinishAll = false;
            blockOnFinish = false;
            cancelled = false;
        }

        public string toString()
        {
            return toStringBuilder().ToString();
        }

        private StringBuilder toStringBuilder(int indent = 0, StringBuilder constructed = null)
        {
            if (constructed == null)
            {
                constructed = new StringBuilder();
            }
            constructed.Append(new string(' ', indent * 3));
            constructed.Append("|");
            constructed.Append(taskName);
            constructed.Append("\n");
            if (sequentialTasks.Count > 0)
            {
                constructed.Append(new string(' ', indent * 3));
                constructed.Append("|->Sequential:\n");
                sequentialTasks[0].toStringBuilder(indent + 1, constructed);
            }
            if (parallelTasks.Count > 0)
            {
                constructed.Append(new string(' ', indent * 3));
                constructed.Append("|->Parallel:\n");
                for (var i = 0; i < parallelTasks.Count; ++i)
                {
                    parallelTasks[i].toStringBuilder(indent + 1, constructed);
                }
            }
            return constructed;
        }
    }
}