#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS

#include <winternl.h>
#include <ntstatus.h>

#include <cassert>
#include <functional>

namespace error {
	inline namespace v0_1_0 {
		template<class T, class IsError = typename T::is_error> bool ok(const T& t) { return !!t; }

		template<class T, class IsError = typename T::is_error>
		class unique_error 
		{
			T error;
			mutable bool issafe;

			void safe_or_terminate() const { if (!issafe) { std::terminate(); } }

		public:
			~unique_error() { safe_or_terminate(); }

			unique_error() : issafe(true) {}
			template<class V>
			unique_error(V v) : error(v), issafe(false) {}

			unique_error(const unique_error& o) : error(o.error), issafe(o.issafe) { o.issafe = true; }
			unique_error& operator=(const unique_error& o) { error = o.error; issafe = o.issafe; o.issafe = true; return *this; }

			bool ok() const { issafe = true;  return error::ok(error); }
			explicit operator bool() const { return ok(); }

			T& get() { return error; }
			const T& get() const { return error; }

			bool is_safe() const { return issafe; }

			unique_error& reset() { safe_or_terminate(); error = T{}; issafe = true; return *this; }
			template<class V>
			unique_error& reset(V v) { safe_or_terminate(); error = T{v}; issafe = false; return *this; }

			T release() { T result = error; error = T{}; issafe = true; return result; }
		};

		template<class T, class IsError = typename T::is_error> bool ok(const unique_error<T>& t) { return t.ok(); }

		struct error_exception : public std::exception 
		{
			bool isok;

			template<class T, class IsError = T::is_error> explicit error_exception(const T& e) : isok(error::ok(e)) {}

			bool ok() const { return isok; }
			explicit operator bool() const { return ok(); }
		};
	}
}
template<class T, class IsError = typename T::is_error> bool operator==(const T& lhs, const T& rhs) {
	return lhs.value == rhs.value;
}
template<class T, class IsError = typename T::is_error> bool operator!=(const T& lhs, const T& rhs) {
	return lhs.value != rhs.value;
}

template<class ReturnT, class T, class IsErrorHandler = typename T::is_error_handler> auto operator||(const ReturnT& result, const T& handler)
	-> decltype(handler(result)) {
	return		handler(result);
}

template<class T, class IsError = typename T::is_error> bool operator==(const error::unique_error<T>& lhs, const T& rhs) {
	return lhs.get().value == rhs.value;
}
template<class T, class IsError = typename T::is_error> bool operator!=(const error::unique_error<T>& lhs, const T& rhs) {
	return lhs.get().value != rhs.value;
}
template<class T, class IsError = typename T::is_error> bool operator==(const T& lhs, const error::unique_error<T>& rhs) {
	return lhs.value == rhs.get().value;
}
template<class T, class IsError = typename T::is_error> bool operator!=(const T& lhs, const error::unique_error<T>& rhs) {
	return lhs.value != rhs.get().value;
}
template<class T, class IsError = typename T::is_error> bool operator==(const error::unique_error<T>& lhs, const error::unique_error<T>& rhs) {
	return lhs.get().value == rhs.get().value;
}
template<class T, class IsError = typename T::is_error> bool operator!=(const error::unique_error<T>& lhs, const error::unique_error<T>& rhs) {
	return lhs.get().value != rhs.get().value;
}

namespace error {
	inline namespace v0_1_0 {
		struct win
		{
			typedef void is_error;

			constexpr inline win() : value(NOERROR) {}
			constexpr inline explicit win(DWORD e) : value(e) {}

			inline explicit operator bool () const { return value == 0; }

			DWORD value;
		};

		template<class T>
		struct last_error_if_t
		{
			typedef void is_error_handler;

			T invalid;

			explicit last_error_if_t(T invalid) : invalid(invalid) {}

			inline std::pair<win, T> operator()(T r) const { return std::make_pair(win{ (r == invalid) ? GetLastError() : NOERROR }, r); }
		};
		template<class T>
		last_error_if_t<T> last_error_if(T invalid) { return last_error_if_t<T>(invalid); }

		struct win_exception : public error_exception
		{
			win error;

			explicit win_exception(const win& e) : error_exception(e), error(e) {}
		};

		template<class T>
		struct throw_last_error_if_t
		{
			typedef void is_error_handler;

			T invalid;

			explicit throw_last_error_if_t(T invalid) : invalid(invalid) {}

			inline T operator()(T r) const { if (r != invalid) return r; throw win_exception{ win{ GetLastError() } }; }
		};
		template<class T>
		throw_last_error_if_t<T> throw_last_error_if(T invalid) { return throw_last_error_if_t<T>(invalid); }
	}
	inline namespace literals {
		inline namespace win_literals {
			constexpr error::win operator "" _win(unsigned long long err) {
				return error::win{ DWORD(err) };
			}
		}
	}
}
namespace error {
	inline namespace v0_1_0 {
		struct nt
		{
			typedef void is_error;

			constexpr inline nt() : value(STATUS_SUCCESS) {}
			constexpr inline explicit nt(NTSTATUS nt) : value(nt) {}

			inline explicit operator bool() const { return error(); }

			bool success() const {
				return NT_SUCCESS(value);
			}
			bool information() const {
				return NT_INFORMATION(value);
			}
			bool warning() const {
				return NT_WARNING(value);
			}
			bool error() const {
				return NT_ERROR(value);
			}

			NTSTATUS value;
		};
		struct nt_exception : public error_exception
		{
			nt error;

			explicit nt_exception(const nt& e) : error_exception(e), error(e) {}
		};
		struct throw_nt_t
		{
			typedef void is_error_handler;

			inline void operator()(NTSTATUS v) const { auto nte = nt{ v }; if (nte) return; throw nt_exception{ nte }; }
		};
		throw_nt_t throw_nt{};
	}
	inline namespace literals {
		inline namespace nt_literals {
			constexpr error::nt operator "" _nt(unsigned long long nt) {
				return error::nt{ NTSTATUS(nt) };
			}
		}
	}
}
namespace error {
	inline namespace v0_1_0 {
		struct hr
		{
			typedef void is_error;

			constexpr inline hr() : value(S_OK) {}
			constexpr inline explicit hr(HRESULT hr) : value(hr) {}

			inline explicit operator bool() const { return succeeded(); }

			bool succeeded() const {
				return SUCCEEDED(value);
			}
			bool failed() const {
				return FAILED(value);
			}

			HRESULT value;
		};
		struct hr_exception : public error_exception
		{
			hr error;

			explicit hr_exception(const hr& e) : error_exception(e), error(e) {}
		};
		struct throw_hr_t
		{
			typedef void is_error_handler;

			inline void operator()(HRESULT v) const { auto hre = hr{ v }; if (hre) return; throw hr_exception{ hre }; }
		};
		throw_hr_t throw_hr{};
	}
	inline namespace literals {
		inline namespace hr_literals {
			constexpr error::hr operator "" _hr(unsigned long long hr) {
				return error::hr{ HRESULT(hr) };
			}
		}
	}
}

namespace e = error;
using namespace error;

int wmain() {

	if (ok(win{ NOERROR })) {
	}

	if (ok(nt{ STATUS_SUCCESS })) {
	}

	if (ok(hr{ S_OK })) {
	}

	if (ok(unique_error<hr>{ S_OK })) {
	}

	if (ok(0_hr)) {
	}

	if (ok(0_nt)) {
	}

	if (ok(0_win)) {
	}

	if (0_hr != 1_hr) {
	}


	{
		HANDLE event = nullptr;
		event = CreateEvent(nullptr, TRUE, TRUE, nullptr) || throw_last_error_if(HANDLE(NULL));
		CloseHandle(event);
	}

	{
		unique_error<win> err;
		HANDLE event = nullptr;

		std::tie(err, event) = CreateEvent(nullptr, TRUE, TRUE, nullptr) || last_error_if(HANDLE(NULL));
		assert(!err.is_safe());
		if (!err) // checking makes err safe
		{
			return -1;
		}
		CloseHandle(event);
	}

	{
		unique_error<win> err;
		HANDLE event = nullptr;

		std::tie(err, event) = CreateEvent(nullptr, TRUE, TRUE, nullptr) || last_error_if(HANDLE(NULL));
		assert(!err.is_safe());
		if (err == win{ NOERROR }) {
			// use event
		}
		auto we = err.release(); // make err safe
		assert(err.is_safe());
		CloseHandle(event);
	}

	{
		CLSID clsid = {};
		CoCreateGuid(&clsid) || e::throw_hr;
	}

	{
		unique_error<hr> hres;
		CLSID clsid = {};
		hres.reset(CoCreateGuid(&clsid));
		assert(!hres.is_safe());
		if (!hres) // checking makes hres safe
		{
			return -1;
		}
	}

	{
		unique_error<hr> hres;
		CLSID clsid = {};
		hres.reset(CoCreateGuid(&clsid));
		assert(!hres.is_safe());
		if (hres == hr{ S_OK }) {
			// use event
		}
		auto hre = hres.release(); // make hres safe
		assert(hres.is_safe());
	}

#if 0
	{
		unique_error<win> err;
		HANDLE event = nullptr;

		std::tie(err, event) = CreateEvent(nullptr, TRUE, TRUE, nullptr) || last_error_if(HANDLE(NULL));
		assert(!err.is_safe());
		CloseHandle(event);
		// terminate called here because err is not checked or released
	}

	{
		unique_error<win> err;
		HANDLE event = nullptr;

		std::tie(err, event) = CreateEvent(nullptr, TRUE, TRUE, nullptr) || last_error_if(HANDLE(NULL));
		assert(!err.is_safe());
		CloseHandle(event);

		// terminate called here because err is not checked
		std::tie(err, event) = CreateEvent(nullptr, TRUE, TRUE, nullptr) || last_error_if(HANDLE(NULL));
		assert(!err.is_safe());
		if (ok(err))
		{
			return -1;
		}
		CloseHandle(event);
	}
#endif
#if 0
	if (ok(1)) {
		return -1;
	}
#endif
#if 0
	if (0_hr == 0_nt) {
	}
#endif
}
