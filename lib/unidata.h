/* Automatically generated file, see scripts/make-unidata */
#ifndef UNIDATA_H
#define UNIDATA_H
enum unicode_General_Category {
  unicode_General_Category_Cc,
  unicode_General_Category_Cf,
  unicode_General_Category_Cn,
  unicode_General_Category_Co,
  unicode_General_Category_Cs,
  unicode_General_Category_Ll,
  unicode_General_Category_Lm,
  unicode_General_Category_Lo,
  unicode_General_Category_Lt,
  unicode_General_Category_Lu,
  unicode_General_Category_Mc,
  unicode_General_Category_Me,
  unicode_General_Category_Mn,
  unicode_General_Category_Nd,
  unicode_General_Category_Nl,
  unicode_General_Category_No,
  unicode_General_Category_Pc,
  unicode_General_Category_Pd,
  unicode_General_Category_Pe,
  unicode_General_Category_Pf,
  unicode_General_Category_Pi,
  unicode_General_Category_Po,
  unicode_General_Category_Ps,
  unicode_General_Category_Sc,
  unicode_General_Category_Sk,
  unicode_General_Category_Sm,
  unicode_General_Category_So,
  unicode_General_Category_Zl,
  unicode_General_Category_Zp,
  unicode_General_Category_Zs
};
enum unicode_Grapheme_Break {
  unicode_Grapheme_Break_CR,
  unicode_Grapheme_Break_Control,
  unicode_Grapheme_Break_Extend,
  unicode_Grapheme_Break_L,
  unicode_Grapheme_Break_LF,
  unicode_Grapheme_Break_LV,
  unicode_Grapheme_Break_LVT,
  unicode_Grapheme_Break_Other,
  unicode_Grapheme_Break_T,
  unicode_Grapheme_Break_V
};
extern const char *const unicode_Grapheme_Break_names[];
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
enum unicode_Sentence_Break {
  unicode_Sentence_Break_ATerm,
  unicode_Sentence_Break_Close,
  unicode_Sentence_Break_Extend,
  unicode_Sentence_Break_Format,
  unicode_Sentence_Break_Lower,
  unicode_Sentence_Break_Numeric,
  unicode_Sentence_Break_OLetter,
  unicode_Sentence_Break_Other,
  unicode_Sentence_Break_STerm,
  unicode_Sentence_Break_Sep,
  unicode_Sentence_Break_Sp,
  unicode_Sentence_Break_Upper
};
extern const char *const unicode_Sentence_Break_names[];
enum unicode_flags {
  unicode_normalize_before_casefold = 1
};

struct unidata {
  const uint32_t *compat;
  const uint32_t *canon;
  const uint32_t *casefold;
  unsigned char ccc;
  char general_category;
  uint8_t flags;
  char grapheme_break;
  char word_break;
  char sentence_break;
};
extern const struct unidata *const unidata[];
extern const struct unicode_utf8_row {
  uint8_t count;
  uint8_t min2, max2;
} unicode_utf8_valid[];
#define UNICODE_NCHARS 1114112
#define UNICODE_MODULUS 16
#define UNICODE_BREAK_START 196608
#define UNICODE_BREAK_END 917504
#define UNICODE_BREAK_TOP 918016
#endif
