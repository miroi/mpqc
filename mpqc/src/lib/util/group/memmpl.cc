
#ifndef _util_group_memmpl_cc
#define _util_group_memmpl_cc

#ifdef __GNUC__
#pragma implementation
#endif

#include <unistd.h>
#include <util/group/memmpl.h>

#include <mpi.h>
#include <mpproto.h>

///////////////////////////////////////////////////////////////////////
// The handler function and its data

static volatile int global_source, global_type, global_mid;
static MPLMemoryGrp *global_mpl_mem = 0;

static void
mpl_memory_handler(int*msgid_arg)
{
  long lmid = *msgid_arg;
  if (!global_mpl_mem) {
      fprintf(stderr,"WARNING: Tried to call mpl_memory_handler"
              " without global_mpl_mem\n");
    }
  else {
      global_mpl_mem->handler(&lmid);
    }
}

///////////////////////////////////////////////////////////////////////
// The MPLMemoryGrp class

#define CLASSNAME MPLMemoryGrp
#define HAVE_KEYVAL_CTOR
#define PARENTS public MIDMemoryGrp
#include <util/class/classi.h>
void *
MPLMemoryGrp::_castdown(const ClassDesc*cd)
{
  void* casts[1];
  casts[0] =  MIDMemoryGrp::_castdown(cd);
  return do_castdowns(casts,cd);
}

long
MPLMemoryGrp::lock()
{
  int oldvalue;
  mpc_lockrnc(1, &oldvalue);
  return oldvalue;
}

void
MPLMemoryGrp::unlock(long oldvalue)
{
  int old = oldvalue;
  mpc_lockrnc(old, &old);
}

long
MPLMemoryGrp::send(void* data, int nbytes, int node, int type)
{
  int mid;
  mpc_send(data, nbytes, node, type, &mid);
  return mid;
}

long
MPLMemoryGrp::recv(void* data, int nbytes, int node, int type)
{
  int n;
  if (node == -1) n = DONTCARE;
  else n = node;
  int t = type;
  int mid;
  mpc_recv(data, nbytes, &n, &t, &mid);
  if (debug_) printf("MPLMemoryGrp:: recv mid = %d\n", mid);
  return mid;
}

long
MPLMemoryGrp::postrecv(void *data, int nbytes, int type)
{
  global_type = type;
  global_source = DONTCARE; 
  mpc_rcvncall(data, nbytes,
               (int*)&global_source, (int*)&global_type, (int*)&global_mid,
               mpl_memory_handler);
  if (debug_) printf("MPLMemoryGrp:: postrecv mid = %d\n", global_mid);
  return global_mid;
}

long
MPLMemoryGrp::wait(long mid1, long mid2)
{
  int imid;
  if (mid2 == -1) imid = (int)mid1;
  else imid = DONTCARE;
  size_t count;
  if (debug_) printf("MPLMemoryGrp: waiting on %d\n", imid);
  if (mpc_wait(&imid,&count)) {
      fprintf(stderr, "MPLMemoryGrp: mpc_wait failed\n");
      sleep(1);
      abort();
    }
  if (debug_) printf("MPLMemoryGrp: imid = %d, global_mid = %d "
                     "DONTCARE = %d count = %d\n",
                     imid, global_mid, DONTCARE, count);
  return (long)imid;
}

MPLMemoryGrp::MPLMemoryGrp(const RefMessageGrp& msg):
  MIDMemoryGrp(msg)
{
  if (debug_) printf("MPLMemoryGrp entered\n");
  if (global_mpl_mem) {
      fprintf(stderr, "MPLMemoryGrp: only one allowed at a time\n");
      sleep(1);
      abort();
    }

  global_mpl_mem = this;

  use_acknowledgments_ = 0;
  use_active_messages_ = 1;

  if (debug_) printf("MPLMemoryGrp activating\n");
  activate();
  if (debug_) printf("MPLMemoryGrp done\n");
}

MPLMemoryGrp::MPLMemoryGrp(const RefKeyVal& keyval):
  MIDMemoryGrp(keyval)
{
  if (debug_) printf("MPLMemoryGrp KeyVal entered\n");
  if (global_mpl_mem) {
      fprintf(stderr, "MPLMemoryGrp: only one allowed at a time\n");
      sleep(1);
      abort();
    }

  global_mpl_mem = this;

  if (debug_) printf("MPLMemoryGrp activating\n");
  activate();
  if (debug_) printf("MPLMemoryGrp done\n");
}

MPLMemoryGrp::~MPLMemoryGrp()
{
  if (debug_) printf("MPLMemoryGrp: in DTOR\n");
  deactivate();

  int oldlock = lock();
  global_mpl_mem = 0;
  unlock(oldlock);
}

void
MPLMemoryGrp::deactivate()
{
  if (!global_mpl_mem) return;
  MIDMemoryGrp::deactivate();
}

#endif
