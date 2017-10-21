#ifndef _MV_SCOPEGUARD_H_
#define _MV_SCOPEGUARD_H_

//Lifted from Alexandrescu's ScopeGuard11 talk.

namespace MV {
	template <typename Fun>
	class ScopeGuard {
		Fun f_;
		bool active_;
	public:
		ScopeGuard(Fun f)
			: f_(std::move(f))
			, active_(true) {
		}
		~ScopeGuard() { if(active_) f_(); }
		void dismiss() { active_ = false; }
		ScopeGuard() = delete;
		ScopeGuard(const ScopeGuard&) = delete;
		ScopeGuard& operator=(const ScopeGuard&) = delete;
		ScopeGuard(ScopeGuard&& rhs)
			: f_(std::move(rhs.f_))
			, active_(rhs.active_) {
			rhs.dismiss();
		}
	};

	template<typename Fun>
	ScopeGuard<Fun> scopeGuard(Fun f){
		return ScopeGuard<Fun>(std::move(f));
	}

	namespace ScopeMacroSupport {
		enum class ScopeGuardOnExit {};
		template <typename Fun>
		MV::ScopeGuard<Fun> operator+(ScopeGuardOnExit, Fun&& fn) {
			return MV::ScopeGuard<Fun>(std::forward<Fun>(fn));
		}
	}

#define SCOPE_EXIT \
	auto ANONYMOUS_VARIABLE(SCOPE_EXIT_STATE) \
	= MV::ScopeMacroSupport::ScopeGuardOnExit() + [&]()

#define CONCATENATE_IMPL(s1, s2) s1##s2
#define CONCATENATE(s1, s2) CONCATENATE_IMPL(s1, s2)
#ifdef __COUNTER__
#define ANONYMOUS_VARIABLE(str) \
	CONCATENATE(str, __COUNTER__)
#else
#define ANONYMOUS_VARIABLE(str) \
	CONCATENATE(str, __LINE__)
#endif
}

#endif
