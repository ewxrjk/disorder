#ifndef UNIDATA_H
#define UNIDATA_H
enum unicode_gc_cat {
  unicode_gc_Cc,
  unicode_gc_Cf,
  unicode_gc_Cn,
  unicode_gc_Co,
  unicode_gc_Cs,
  unicode_gc_Ll,
  unicode_gc_Lm,
  unicode_gc_Lo,
  unicode_gc_Lt,
  unicode_gc_Lu,
  unicode_gc_Mc,
  unicode_gc_Me,
  unicode_gc_Mn,
  unicode_gc_Nd,
  unicode_gc_Nl,
  unicode_gc_No,
  unicode_gc_Pc,
  unicode_gc_Pd,
  unicode_gc_Pe,
  unicode_gc_Pf,
  unicode_gc_Pi,
  unicode_gc_Po,
  unicode_gc_Ps,
  unicode_gc_Sc,
  unicode_gc_Sk,
  unicode_gc_Sm,
  unicode_gc_So,
  unicode_gc_Zl,
  unicode_gc_Zp,
  unicode_gc_Zs
};
struct unidata {
  enum unicode_gc_cat gc;
  int ccc;
  int upper_offset;
  int lower_offset;
};
extern const struct unidata *const unidata[];
#define UNICODE_NCHARS 195328
#endif