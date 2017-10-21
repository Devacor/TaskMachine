#ifndef __MV_SIGNAL_H__
#define __MV_SIGNAL_H__

#include <atomic>
#include <memory>
#include <utility>
#include <functional>
#include <vector>
#include <set>
#include <string>
#include <map>
#include "Utility/tupleHelpers.hpp"
#include "Utility/scopeGuard.hpp"

namespace MV {

	template <typename T>
	class Receiver {
	public:
		typedef std::function<T> FunctionType;
		typedef std::shared_ptr<Receiver<T>> SharedType;
		typedef std::weak_ptr<Receiver<T>> WeakType;

		static std::shared_ptr< Receiver<T> > make(std::function<T> a_callback){
			return std::shared_ptr< Receiver<T> >(new Receiver<T>(a_callback, ++uniqueId));
		}

		template <class ...Arg>
		void notify(Arg &&... a_parameters){
			if(!blocked() && !invalid()){
				callback(std::forward<Arg>(a_parameters)...);
			}
		}

		template <class ...Arg>
		bool predicate(Arg &&... a_parameters) {
			if (!blocked() && !invalid()) {
				return callback(std::forward<Arg>(a_parameters)...);
			}
			return false;
		}

		bool invalid() const {
			return !callback;
		}

		template <class ...Arg>
		void operator()(Arg &&... a_parameters){
			if(!blocked() && !invalid()){
				callback(std::forward<Arg>(a_parameters)...);
			}
		}

		template <class ...Arg>
		bool predicate() {
			if (!blocked() && !invalid()) {
				return callback();
			}
			return false;
		}
		template <class ...Arg>
		void notify(){
			if(!blocked() && !invalid()){
				callback();
			}
		}
		template <class ...Arg>
		void operator()(){
			if(!blocked() && !invalid()){
				callback();
			}
		}

		void block(){
			++isBlocked;
		}
		void unblock(){
			--isBlocked;
			if (isBlocked < 0) {
				isBlocked = 0;
			}
		}
		bool blocked() const{
			return isBlocked != 0;
		}

		//For sorting and comparison (removal/avoiding duplicates)
		bool operator<(const Receiver<T>& a_rhs){
			return id < a_rhs.id;
		}
		bool operator>(const Receiver<T>& a_rhs){
			return id > a_rhs.id;
		}
		bool operator==(const Receiver<T>& a_rhs){
			return id == a_rhs.id;
		}
		bool operator!=(const Receiver<T>& a_rhs){
			return id != a_rhs.id;
		}

	private:
		Receiver() {
		}
		Receiver(std::function<T> a_callback, uint64_t a_id):
			callback(a_callback),
            id(a_id){
		}

		int isBlocked = 0;
		std::function< T > callback;

		uint64_t id;
		static std::atomic<int64_t> uniqueId;
	};

	template <typename T>
	std::atomic<int64_t> Receiver<T>::uniqueId = 0;

	template <typename T>
	class Signal {
	public:
		typedef std::function<T> FunctionType;
		typedef Receiver<T> RecieverType;
		typedef std::shared_ptr<Receiver<T>> SharedRecieverType;
		typedef std::weak_ptr<Receiver<T>> WeakRecieverType;

		//No protection against duplicates.
		[[nodiscard]] std::shared_ptr<Receiver<T>> connect(std::function<T> a_callback){
			if(observerLimit == std::numeric_limits<size_t>::max() || cullDeadObservers() < observerLimit){
				auto signal = Receiver<T>::make(a_callback);
				observers.insert(signal);
				return signal;
			} else {
				return nullptr;
			}
		}

		//Duplicate Recievers will not be added. If std::function ever becomes comparable this can all be much safer.
		bool connect(std::shared_ptr<Receiver<T>> a_value){
			if(observerLimit == std::numeric_limits<size_t>::max() || cullDeadObservers() < observerLimit){
				observers.insert(a_value);
				return true;
			}else{
				return false;
			}
		}

		//Add owned connections. Note: these should be disconnected via ID instead of by the receiver.
		std::shared_ptr<Receiver<T>> connect(const std::string &a_id, std::function<T> a_callback) {
			return ownedConnections[a_id] = connect(a_callback);
		}

		std::shared_ptr<Receiver<T>> connection(const std::string &a_id) {
			auto foundConnection = ownedConnections.find(a_id);
			if (foundConnection != ownedConnections.end()) {
				return foundConnection->second;
			}
			return SharedRecieverType();
		}

		void disconnect(std::shared_ptr<Receiver<T>> a_value){
			if(a_value){
				if(!inCall){
					observers.erase(a_value);
				} else {
					disconnectQueue.insert(a_value);
				}
			}
		}

		bool connected(const std::string &a_id) {
			return ownedConnections.find(a_id) != ownedConnections.end();
		}

		void disconnect(const std::string &a_id) {
			auto connectionToRemove = ownedConnections.find(a_id);
			if (connectionToRemove != ownedConnections.end()) {
				disconnect(connectionToRemove->second);
				ownedConnections.erase(connectionToRemove);
			}
		}

		void clearObservers(){
			ownedConnections.clear();
			if (!inCall) {
				observers.clear();
			}else{
				disconnectQueue.clear();
				for (auto&& observer : observers) {
					if (auto lockedObserver = observer.lock()) {
						disconnectQueue.insert(lockedObserver);
					}
				}
			}
		}

		void clear(){
			clearObservers();
		}

		void block() {
			if (isBlocked++ == 0) {
				calledWhileBlocked = false;
			}
		}

		bool unblock() {
			if (--isBlocked == 0) {
				return calledWhileBlocked;
			}
			if (isBlocked < 0) {
				isBlocked = 0;
			}
			return false;
		}

		bool blocked() const {
			return isBlocked != 0;
		}

		template <typename ...Arg>
		void operator()(Arg &&... a_parameters){
			if (!blocked()) {
				inCall = true;
				SCOPE_EXIT{
					inCall = false;
					for (auto&& i : disconnectQueue) {
						observers.erase(i);
					}
					disconnectQueue.clear();
				};

				for (auto i = observers.begin(); !observers.empty() && i != observers.end();) {
					if (auto lockedI = i->lock()) {
						auto next = i;
						++next;
						lockedI->notify(std::forward<Arg>(a_parameters)...);
						i = next;
					} else {
						observers.erase(i++);
					}
				}
			}

			if (blocked()) {
				calledWhileBlocked = true;
				if (blockedCallback) {
					blockedCallback(std::forward<Arg>(a_parameters)...);
				}
			}
		}

		template <typename ...Arg>
		void operator()(){
			if (!blocked()) {
				inCall = true;
				SCOPE_EXIT{
					inCall = false;
					for (auto&& i : disconnectQueue) {
						observers.erase(i);
					}
					disconnectQueue.clear();
				};

				for (auto i = observers.begin(); i != observers.end();) {
					if (auto lockedI = i->lock()) {
						auto next = i;
						++next;
						lockedI->notify();
						i = next;
					} else {
						observers.erase(i++);
					}
				}
			}

			if (blocked()){
				calledWhileBlocked = true;
				if (blockedCallback) {
					blockedCallback();
				}
			}
		}

		void setObserverLimit(size_t a_newLimit){
			observerLimit = a_newLimit;
		}
		void clearObserverLimit(){
			observerLimit = std::numeric_limits<size_t>::max();
		}
		int getObserverLimit(){
			return observerLimit;
		}

		size_t cullDeadObservers(){
			for(auto i = observers.begin();!observers.empty() && i != observers.end();) {
				if(i->expired()) {
					observers.erase(i++);
				} else {
					++i;
				}
			}
			return observers.size();
		}

	private:
		std::set< std::weak_ptr< Receiver<T> >, std::owner_less<std::weak_ptr<Receiver<T>>> > observers;
		size_t observerLimit = std::numeric_limits<size_t>::max();
		bool inCall = false;
		int isBlocked = 0;
		std::function<T> blockedCallback;
		std::set< std::shared_ptr<Receiver<T>> > disconnectQueue;
		bool calledWhileBlocked = false;

		std::map<std::string, SharedRecieverType> ownedConnections;
	};

	//Can be used as a public SignalRegister member for connecting signals to a private Signal member.
	//In this way you won't have to write forwarding connect/disconnect boilerplate for your classes.
	template <typename T>
	class SignalRegister {
	public:
		typedef std::function<T> FunctionType;
		typedef Receiver<T> RecieverType;
		typedef std::shared_ptr<Receiver<T>> SharedRecieverType;
		typedef std::weak_ptr<Receiver<T>> WeakRecieverType;

		SignalRegister(Signal<T> &a_signal) :
			signal(a_signal){
		}

		SignalRegister(SignalRegister<T> &a_rhs) :
			signal(a_rhs.signal) {
		}

		//no protection against duplicates
		[[nodiscard]] std::shared_ptr<Receiver<T>> connect(std::function<T> a_callback){
			return signal.connect(a_callback);
		}
		//duplicate shared_ptr's will not be added
		bool connect(std::shared_ptr<Receiver<T>> a_value){
			return signal.connect(a_value);
		}

		void disconnect(std::shared_ptr<Receiver<T>> a_value){
			signal.disconnect(a_value);
		}

		std::shared_ptr<Receiver<T>> connect(const std::string &a_id, std::function<T> a_callback){
			return signal.connect(a_id, a_callback);
		}

		bool connected(const std::string &a_id) {
			return signal.connected(a_id);
		}

		void disconnect(const std::string &a_id){
			signal.disconnect(a_id);
		}

		std::shared_ptr<Receiver<T>> connection(const std::string &a_id){
			return signal.connection(a_id);
		}

		void parameterNames(const std::vector<std::string> &a_orderedParameterNames) {
			signal.parameterNames(a_orderedParameterNames);
		}

		std::vector<std::string> parameterNames() const {
			return signal.parameterNames();
		}

		bool hasParameterNames() const {
			return signal.hasParameterNames();
		}

	private:
		Signal<T> &signal;
	};

}

#endif
