#ifndef _PASSENGER_UTILS_H_
#define _PASSENGER_UTILS_H_

#include <boost/shared_ptr.hpp>
#include <string>
#include <vector>
#include <sstream>

namespace Passenger {

using namespace std;
using namespace boost;

/**
 * Convenience shortcut for creating a <tt>shared_ptr</tt>.
 * Instead of:
 * @code
 *    shared_ptr<Foo> foo;
 *    ...
 *    foo = shared_ptr<Foo>(new Foo());
 * @endcode
 * one can write:
 * @code
 *    shared_ptr<Foo> foo;
 *    ...
 *    foo = ptr(new Foo());
 * @endcode
 *
 * @param pointer The item to put in the shared_ptr object.
 * @ingroup Support
 */
template<typename T> shared_ptr<T>
ptr(T *pointer) {
	return shared_ptr<T>(pointer);
}

/**
 * Used internally by toString(). Do not use directly.
 */
template<typename T>
struct AnythingToString {
	string operator()(T something) {
		stringstream s;
		s << something;
		return s.str();
	}
};

/**
 * Used internally by toString(). Do not use directly.
 */
template<>
struct AnythingToString< vector<string> > {
	string operator()(const vector<string> &v) {
		string result("[");
		vector<string>::const_iterator it;
		unsigned int i;
		for (it = v.begin(), i = 0; it != v.end(); it++, i++) {
			result.append("'");
			result.append(*it);
			if (i == v.size() - 1) {
				result.append("'");
			} else {
				result.append("', ");
			}
		}
		result.append("]");
		return result;
	}
};

/**
 * Convert anything to a string.
 *
 * @param something The thing to convert.
 * @ingroup Support
 */
template<typename T> string
toString(T something) {
	return AnythingToString<T>()(something);
}

/**
 * Converts the given string to an integer.
 * @ingroup Support
 */
int atoi(const string &s);

/**
 * Split the given string using the given separator.
 *
 * @param str The string to split.
 * @param sep The separator to use.
 * @param output The vector to write the output to.
 * @ingroup Support
 */
void split(const string &str, char sep, vector<string> &output);

/**
 * Check whether the specified file exists.
 *
 * @param filename The filename to check.
 * @return Whether the file exists.
 * @ingroup Support
 */
bool fileExists(const char *filename);

/**
 * Find the location of the Passenger spawn server script.
 * This is done by scanning $PATH. For security reasons, only
 * absolute paths are scanned.
 *
 * @return An absolute path to the spawn server script, or
 *         an empty string on error.
 * @ingroup Support
 */
string findSpawnServer();

/**
 * Returns a canonical version of the specified path. All symbolic links
 * and relative path elements are resolved.
 * Returns an empty string if something went wrong.
 *
 * @ingroup Support
 */
string canonicalizePath(const string &path);

/**
 * Check whether the specified directory is a valid Ruby on Rails
 * 'public' directory.
 *
 * @ingroup Support
 */
bool verifyRailsDir(const string &dir);

} // namespace Passenger

#endif /* _PASSENGER_UTILS_H_ */

