
#ifdef __GNUC__
#pragma interface
#endif

#ifndef _chemistry_qc_intv3_storage_h
#define _chemistry_qc_intv3_storage_h

#include <stddef.h>
#include <util/class/class.h>
#include <util/keyval/keyval.h>
#include <util/container/avl.h>

    void int_storage(int);
    void int_done_storage();
    int int_have_stored_integral(int,int,int,int,int,int,int);
    void int_store_integral(int,int,int,int,int,int,int,int);

// the max shell number is 2^15 (sizeof(int) must be >= 4)
#define SH_BITS 15 // the number of bits holding a shell index
#define PE_BITS 1  // the number of bits holding a permutation

#define SH_MASK ((1<<SH_BITS)-1)
#define PE_MASK ((1<<PE_BITS)-1)

#define SH0_SHIFT 0
#define SH1_SHIFT (SH_BITS + SH0_SHIFT)
#define P12_SHIFT (SH_BITS + SH1_SHIFT)
#define P34_SHIFT (PE_BITS + P12_SHIFT)
#define SH2_SHIFT 0
#define SH3_SHIFT (SH_BITS + SH2_SHIFT)
#define P13P24_SHIFT (SH_BITS + SH3_SHIFT)
class IntegralKey {
  public:
    unsigned int sh0_sh1_p12_p34;
    unsigned int sh2_sh3_p13p24;
  public:
    IntegralKey(int,int,int,int,int,int,int);
    IntegralKey(const IntegralKey&);
    int sh0() const { return (sh0_sh1_p12_p34>>SH0_SHIFT) & SH_MASK; }
    int sh1() const { return (sh0_sh1_p12_p34>>SH1_SHIFT) & SH_MASK; }
    int p12() const { return (sh0_sh1_p12_p34>>P12_SHIFT) & PE_MASK; }
    int p34() const { return (sh0_sh1_p12_p34>>P34_SHIFT) & PE_MASK; }
    int sh2() const { return (sh2_sh3_p13p24>>SH2_SHIFT) & SH_MASK; }
    int sh3() const { return (sh2_sh3_p13p24>>SH3_SHIFT) & SH_MASK; }
    int p13p24() const { return (sh2_sh3_p13p24>>P13P24_SHIFT) & PE_MASK; }
};

inline
IntegralKey::IntegralKey(int sh1_, int sh2_, int sh3_, int sh4_,
                         int p12_, int p34_, int p13p24_)
{
  sh0_sh1_p12_p34 = (sh1_<<SH0_SHIFT)
                    |(sh2_<<SH1_SHIFT)
                    |(p12_<<P12_SHIFT)
                    |(p34_<<P34_SHIFT);
  sh2_sh3_p13p24 = (sh3_<<SH2_SHIFT)
                   |(sh4_<<SH3_SHIFT)
                   |(p13p24_<<P13P24_SHIFT);
}

inline
IntegralKey::IntegralKey(const IntegralKey& ik)
{
  sh0_sh1_p12_p34 = ik.sh0_sh1_p12_p34;
  sh2_sh3_p13p24 = ik.sh2_sh3_p13p24;
}

inline int
compare(const IntegralKey&k1, const IntegralKey&k2)
{
  if (k1.sh0_sh1_p12_p34 < k2.sh0_sh1_p12_p34) return -1;
  else if (k1.sh0_sh1_p12_p34 > k2.sh0_sh1_p12_p34) return 1;

  if (k1.sh2_sh3_p13p24 < k2.sh2_sh3_p13p24) return -1;
  else if (k1.sh2_sh3_p13p24 > k2.sh2_sh3_p13p24) return 1;
  else return 0;
}

class IntegralLink {
  public:
    EAVLNode<IntegralLink, IntegralKey> intlist;
    EAVLNode<IntegralLink, int> costlist;
    int size;
  public:
    IntegralLink(IntegralKey& key, int cost, int size);
    static int size_to_actualsize(int size);
    ~IntegralLink();
    int actualsize() const;
    int hash() const;
    static int shells_to_hash(int,int,int,int);
    int cost() const { return costlist.key; }
    void print();

    // the integrals are squirreled away after this
    double* buffer() { return (double*)&this[1]; }
    void* operator new(size_t, int);
    void operator delete(void*);
};

inline int
IntegralLink::shells_to_hash(int sh1,int sh2,int sh3,int sh4)
{
  return sh1 ^ (sh4<<4) ^ (sh2<<8) ^ (sh3<<12);
}

inline int
IntegralLink::hash() const
{
  return shells_to_hash(intlist.key.sh0(),
                        intlist.key.sh1(),
                        intlist.key.sh2(),
                        intlist.key.sh3());
}

inline int
IntegralLink::size_to_actualsize(int size)
{
  return size*sizeof(double) + sizeof(IntegralLink) + sizeof(void*)*2;
}

inline int
IntegralLink::actualsize() const
{
  return size_to_actualsize(size);
}

class IntegralStorer: public DescribedClass {
#   define CLASSNAME IntegralStorer
#   define HAVE_CTOR
#   define HAVE_KEYVAL_CTOR
#   include <util/class/classd.h>
  private:
    int table_size_;
    EAVLList<IntegralLink,int> costlist;
    EAVLList<IntegralLink,IntegralKey>* table_;
    int maxsize_;
    int currentsize_;
    int n_integrals_;
    int n_shellquart_;
  public:
    IntegralStorer();
    IntegralStorer(const RefKeyVal&);
    ~IntegralStorer();
    void init(int nbyte);
    void done();
    IntegralLink *find(IntegralKey&);
    int should_store(int cost, int actualsize);
    void store(IntegralKey& key, double* buf,
               int size, int cost, int actualsize);
    void print_stats();
    int table_size() const { return table_size_; }
};
DescribedClass_REF_dec(IntegralStorer);

#endif
