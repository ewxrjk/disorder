/* Automatically generated file, see scripts/make-unidata */
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
enum unicode_Word_Break {
  unicode_Word_Break_ALetter,
  unicode_Word_Break_Extend,
  unicode_Word_Break_ExtendNumLet,
  unicode_Word_Break_Format,
  unicode_Word_Break_Katakana,
  unicode_Word_Break_MidLetter,
  unicode_Word_Break_MidNum,
  unicode_Word_Break_Numeric,
  unicode_Word_Break_Other
};
extern const char *const unicode_Word_Break_names[];
enum unicode_flags {
  unicode_normalize_before_casefold = 1
};

struct unidata {
  const uint32_t *compat;
  const uint32_t *canon;
  const uint32_t *casefold;
  int16_t upper_offset;
  int16_t lower_offset;
  unsigned char ccc;
  char gc;
  uint8_t flags;
  char word_break;
};
extern const struct unidata *const unidata[];
#define UNICODE_NCHARS 195200
#define UNICODE_MODULUS 128
#endif
