#ifndef __TASKACTIONS_HPP__
#define __TASKACTIONS_HPP__

#include <string>
#include "Utility/task.hpp"

namespace MV {
	class BlockForSeconds : public ActionBase {
	public:
		BlockForSeconds(double a_seconds) :
			seconds(a_seconds) {
		}

		std::string name() const override {
			return "BlockForSeconds (" + std::to_string(seconds) + ")";
		}

		bool update(Task &a_self, double a_dt) override {
			return a_self.localElapsed() >= seconds;
		}
	private:
		double seconds = 0.0;
	};

	class BlockForFrames : public ActionBase {
	public:
		BlockForFrames(int a_frames = 1) :
			frames(a_frames) {
		}

		std::string name() const override {
			return "BlockForFrames (" + std::to_string(frames) + ")";
		}

		bool update(Task& a_self, double a_dt) override {
			return totalFrames++ >= frames;
		}

	private:
		int totalFrames = 0;
		int frames = 0;
	};

	class BlockWhile : public ActionBase {
	public:
		BlockWhile(std::function<bool()> a_predicate) :
			predicate(a_predicate) {
		}

		std::string name() const override {
			return "BlockWhile";
		}

		bool update(Task &a_self, double a_dt) override {
			return !predicate();
		}
	private:
		std::function<bool()> predicate;
	};

	class BlockUntil : public ActionBase {
	public:
		BlockUntil(std::function<bool()> a_predicate) :
			predicate(a_predicate) {
		}

		std::string name() const override {
			return "BlockUntil";
		}

		bool update(Task& a_self, double a_dt) override {
			return predicate();
		}
	private:
		std::function<bool()> predicate;
	};
}

#endif
