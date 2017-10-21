#ifndef __MV_TUPLE_HELPERS_HPP__
#define __MV_TUPLE_HELPERS_HPP__

#include <tuple>
#include <string>
#include <memory>

namespace MV {
	namespace Detail {
		template<class T>
		std::reference_wrapper<T> prepareForChaiscript(T& a_value) {
			return{ a_value };
		}
		template<class T>
		T* prepareForChaiscript(T* a_value) {
			return a_value;
		}

		//may be able to convert these to string_view when chaiscript gets support and it becomes standard.
		inline std::string prepareForChaiscript(char* a_cstring) {
			return{ a_cstring };
		}

		inline std::string prepareForChaiscript(const char* a_cstring) {
			return{ a_cstring };
		}

		template<class T>
		T* prepareForChaiscript(std::unique_ptr<T>& a_value) {
			return a_value.get();
		}
		template<class T>
		T* prepareForChaiscript(std::weak_ptr<T>& a_value) {
			return{ !a_value.expired() ? a_value.lock().get() : nullptr };
		}
		template<class T>
		std::shared_ptr<T> prepareForChaiscript(std::shared_ptr<T>& a_value) {
			return{ a_value };
		}
		template<class T>
		const std::shared_ptr<T> prepareForChaiscript(const std::shared_ptr<T>& a_value) {
			return{ a_value };
		}
	}

	template<class T, class Tuple, std::size_t N>
	struct TupleAggregator {
		static void addToVector(const Tuple& t, std::vector<T> &a_aggregate) {
			TupleAggregator<T, Tuple, N - 1>::addToVector(t, a_aggregate);
			a_aggregate.emplace_back(Detail::prepareForChaiscript(std::get<N - 1>(t)));
		}
	};

	template<class T, class Tuple>
	struct TupleAggregator<T, Tuple, 1> {
		static void addToVector(const Tuple& t, std::vector<T> &a_aggregate) {
			a_aggregate.emplace_back(Detail::prepareForChaiscript(std::get<0>(t)));
		}
	};

	//Converts a tuple to a vector of pointers to the elements in the tuple.
	//Works with boost::any or chaiscript::Boxed_Value etc.
	template<class T, class... Args>
	std::vector<T> toVector(const std::tuple<Args...>& t) {
		std::vector<T> result;
		TupleAggregator<T, decltype(t), sizeof...(Args)>::addToVector(t, result);
		return result;
	}

}

#endif