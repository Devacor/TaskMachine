#ifndef __MV_REQUIRE_H__
#define __MV_REQUIRE_H__

#undef require

#include <string>
#include <tuple>
#include <sstream>
#include <stdint.h>
#include <iomanip>

namespace MV {

	inline std::string to_string(int a_type){
		return std::to_string(a_type);
	}
	inline std::string to_string(unsigned int a_type){
		return std::to_string(a_type);
	}
	inline std::string to_string(long a_type){
		return std::to_string(a_type);
	}
	inline std::string to_string(unsigned long a_type){
		return std::to_string(a_type);
	}
  #ifndef __GNUC__
	inline std::string to_string(int64_t a_type){
		return std::to_string(a_type);
	}
	inline std::string to_string(uint64_t a_type){
		return std::to_string(a_type);
	}
  #endif
	inline std::string to_string(long double a_type){
		return std::to_string(a_type);
	}
	inline std::string to_string(double a_type){
		return std::to_string(a_type);
	}
	inline std::string to_string(float a_type){
		return std::to_string(a_type);
	}
	inline std::string& to_string(std::string &a_string){
		return a_string;
	}
	inline const std::string& to_string(const std::string &a_string){
		return a_string;
	}
	inline const char* const to_string(const char * const a_string){
		return a_string;
	}
	inline char* to_string(char *a_string){
		return a_string;
	}
	inline std::string to_string(long double toCast, unsigned precision) {
		std::ostringstream result;
		result << std::setprecision(precision) << toCast;
		return result.str();
	}
	inline std::string to_string(double toCast, unsigned precision) {
		std::ostringstream result;
		result << std::setprecision(precision) << toCast;
		return result.str();
	}
	inline std::string to_string(float toCast, unsigned precision) {
		std::ostringstream result;
		result << std::setprecision(precision) << toCast;
		return result.str();
	}

	template<typename T>
	std::string to_string(const T &a_type) {
		std::stringstream stream;
		stream << a_type;
		return stream.str();
	}

	template<class Tuple, std::size_t N>
	struct TupleStringHelper {
		static void to_string_combiner(const Tuple& t, std::stringstream &a_stream){
			TupleStringHelper<Tuple, N - 1>::to_string_combiner(t, a_stream);
			a_stream << MV::to_string(std::get<N - 1>(t));
		}
	};

	template<class Tuple>
	struct TupleStringHelper<Tuple, 1> {
		static void to_string_combiner(const Tuple& t, std::stringstream &a_stream){
			a_stream << MV::to_string(std::get<0>(t));
		}
	};

	template<class... Args>
	std::string to_string(const std::tuple<Args...>& t){
		std::stringstream stream;
		TupleStringHelper<decltype(t), sizeof...(Args)>::to_string_combiner(t, stream);
		return stream.str();
	}

	class Exception : public virtual std::runtime_error {
	public:
		explicit Exception(const std::string& a_message): std::runtime_error(a_message){}
		explicit Exception(const char *a_message): std::runtime_error(a_message) {}

		virtual const char * what() const noexcept override {
			prefixWhat("General Exception: ");
			return combinedWhat.c_str();
		}
	protected:
		void prefixWhat(const char * a_prefix) const{
			if(combinedWhat.empty()){
				combinedWhat = a_prefix;
				combinedWhat += runtime_error::what();
			}
		}

		mutable std::string combinedWhat;
	};

	class RangeException : public Exception {
	public:
        explicit RangeException(const std::string& a_message): std::runtime_error(a_message), Exception(a_message) {}
		explicit RangeException(const char *a_message): std::runtime_error(a_message), Exception(a_message) {}

		virtual const char * what() const noexcept override {
			prefixWhat("Range Exception: ");
			return combinedWhat.c_str();
		}
	};

	class ResourceException : public Exception {
	public:
		explicit ResourceException(const std::string& a_message): std::runtime_error(a_message), Exception(a_message) {}
		explicit ResourceException(const char *a_message): std::runtime_error(a_message), Exception(a_message) {}

		virtual const char * what() const noexcept override {
			prefixWhat("Resource Exception: ");
			return combinedWhat.c_str();
		}
	};

	class DeviceException : public Exception {
	public:
		explicit DeviceException(const std::string& a_message) : std::runtime_error(a_message), Exception(a_message) {}
		explicit DeviceException(const char *a_message) : std::runtime_error(a_message), Exception(a_message) {}

		virtual const char * what() const noexcept override {
			prefixWhat("Device Exception: ");
			return combinedWhat.c_str();
		}
	};

	class PointerException : public Exception {
	public:
		explicit PointerException(const std::string& a_message): std::runtime_error(a_message), Exception(a_message) {}
		explicit PointerException(const char *a_message): std::runtime_error(a_message), Exception(a_message) {}

		virtual const char * what() const noexcept override {
			prefixWhat("Pointer Exception: ");
			return combinedWhat.c_str();
		}
	};

	class LogicException : public Exception {
	public:
		explicit LogicException(const std::string& a_message) : std::runtime_error(a_message), Exception(a_message) {}
		explicit LogicException(const char *a_message) : std::runtime_error(a_message), Exception(a_message) {}

		virtual const char * what() const noexcept override {
			prefixWhat("Logic Exception: ");
			return combinedWhat.c_str();
		}
	};

	template <typename ExceptionType, typename ConditionType, typename... Args>
	inline void require(ConditionType&& a_condition, Args&&... a_args){
		if(!a_condition){
			ExceptionType exception(MV::to_string(std::make_tuple(std::forward<Args>(a_args)...)));
			std::cerr << "ASSERTION FAILED! " << exception.what() << std::endl;
			throw exception;
		}
	}
}

#endif
