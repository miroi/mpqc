
#ifndef CLASSNAME
#define CLASSNAME you_forgot_to_define_CLASSNAME
#endif

  public:
     // this is public to make it easier to force linkage
    static ClassDesc class_desc_;
  private:
    void* CLASSNAME::do_castdowns(void**,
                                  const ClassDesc*cd);
  public:
    void* _castdown(const ClassDesc*);
    static CLASSNAME* require_castdown(DescribedClass*p,const char*,...);
    static CLASSNAME* castdown(DescribedClass*p);
    static CLASSNAME* castdown(const RefDescribedClass&p);
    static const ClassDesc* static_class_desc();
    const ClassDesc* class_desc() const;
#ifdef HAVE_CTOR
    static DescribedClass* create();
#undef HAVE_CTOR
#endif
#ifdef HAVE_KEYVAL_CTOR
    static DescribedClass* create(const RefKeyVal&);
#undef HAVE_KEYVAL_CTOR
#endif
#ifdef HAVE_STATEIN_CTOR
    static DescribedClass* create(StateIn&);
#undef HAVE_STATEIN_CTOR
#endif
  private:

#undef HAVE_KEYVAL_CTOR
#undef HAVE_STATEIN_CTOR
#undef HAVE_CTOR
#undef CLASSNAME

