/*
 *  Phusion Passenger - http://www.modrails.com/
 *  Copyright (c) 2011, 2012 Phusion
 *
 *  "Phusion Passenger" is a trademark of Hongli Lai & Ninh Bui.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 */
#ifndef _PASSENGER_APPLICATION_POOL2_COMMON_H_
#define _PASSENGER_APPLICATION_POOL2_COMMON_H_

#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <oxt/tracable_exception.hpp>
#include <ApplicationPool2/Options.h>
#include <Utils/StringMap.h>

namespace tut {
	struct ApplicationPool2_PoolTest;
}

namespace Passenger {
namespace ApplicationPool2 {

using namespace std;
using namespace boost;
using namespace oxt;

class Pool;
class SuperGroup;
class Group;
class Process;
class Session;

typedef shared_ptr<Pool> PoolPtr;
typedef shared_ptr<SuperGroup> SuperGroupPtr;
typedef shared_ptr<Group> GroupPtr;
typedef shared_ptr<Process> ProcessPtr;
typedef shared_ptr<Session> SessionPtr;
typedef shared_ptr<tracable_exception> ExceptionPtr;
typedef StringMap<SuperGroupPtr> SuperGroupMap;
typedef function<void (const SessionPtr &session, const ExceptionPtr &e)> GetCallback;
typedef function<void ()> Callback;

struct GetWaiter {
	Options options;
	GetCallback callback;
	
	GetWaiter(const Options &o, const GetCallback &cb)
		: options(o),
		  callback(cb)
	{
		options.persist(o);
	}
};

struct Ticket {
	boost::mutex syncher;
	condition_variable cond;
	SessionPtr session;
	ExceptionPtr exception;
};

ExceptionPtr copyException(const tracable_exception &e);
void rethrowException(const ExceptionPtr &e);

} // namespace ApplicationPool2
} // namespace Passenger

#endif /* _PASSENGER_APPLICATION_POOL2_COMMON_H_ */
