#include "Resumable.h"
#include "Scheduler.h"

coproto::error_code coproto::NoOp::resume_(Scheduler& sched) { sched.fulfillDep(*this, {}, {}); return {}; }
