/* ELF object file format
   Copyright (C) 1992-2018 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3,
   or (at your option) any later version.

   GAS is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
   the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#define OBJ_HEADER "obj-elf.h"
#include "as.h"
#include "safe-ctype.h"
#include "subsegs.h"
#include "obstack.h"
#include "struc-symbol.h"
#include "dwarf2dbg.h"

static void obj_elf_line (int);
static void obj_elf_size (int);
static void obj_elf_type (int);
static void obj_elf_ident (int);
static void obj_elf_weak (int);
static void obj_elf_local (int);
static void obj_elf_visibility (int);
static void obj_elf_symver (int);
static void obj_elf_subsection (int);
static void obj_elf_popsection (int);
static void obj_elf_gnu_attribute (int);
static void obj_elf_tls_common (int);
static void obj_elf_lcomm (int);
static void obj_elf_struct (int);

static const pseudo_typeS elf_pseudo_table[] =
{
  {"comm", obj_elf_common, 0},
  {"common", obj_elf_common, 1},
  {"ident", obj_elf_ident, 0},
  {"lcomm", obj_elf_lcomm, 0},
  {"local", obj_elf_local, 0},
  {"previous", obj_elf_previous, 0},
  {"section", obj_elf_section, 0},
  {"section.s", obj_elf_section, 0},
  {"sect", obj_elf_section, 0},
  {"sect.s", obj_elf_section, 0},
  {"pushsection", obj_elf_section, 1},
  {"popsection", obj_elf_popsection, 0},
  {"size", obj_elf_size, 0},
  {"type", obj_elf_type, 0},
  {"version", obj_elf_version, 0},
  {"weak", obj_elf_weak, 0},

  /* These define symbol visibility.  */
  {"internal", obj_elf_visibility, STV_INTERNAL},
  {"hidden", obj_elf_visibility, STV_HIDDEN},
  {"protected", obj_elf_visibility, STV_PROTECTED},

  /* These are used for stabs-in-elf configurations.  */
  {"line", obj_elf_line, 0},

  /* This is a GNU extension to handle symbol versions.  */
  {"symver", obj_elf_symver, 0},

  /* A GNU extension to change subsection only.  */
  {"subsection", obj_elf_subsection, 0},

  /* These are GNU extensions to aid in garbage collecting C++ vtables.  */
  {"vtable_inherit", obj_elf_vtable_inherit, 0},
  {"vtable_entry", obj_elf_vtable_entry, 0},

  /* A GNU extension for object attributes.  */
  {"gnu_attribute", obj_elf_gnu_attribute, 0},

  /* These are used for dwarf.  */
  {"2byte", cons, 2},
  {"4byte", cons, 4},
  {"8byte", cons, 8},
  /* These are used for dwarf2.  */
  { "file", dwarf2_directive_file, 0 },
  { "loc",  dwarf2_directive_loc,  0 },
  { "loc_mark_labels", dwarf2_directive_loc_mark_labels, 0 },

  /* We need to trap the section changing calls to handle .previous.  */
  {"data", obj_elf_data, 0},
  {"offset", obj_elf_struct, 0},
  {"struct", obj_elf_struct, 0},
  {"text", obj_elf_text, 0},

  {"tls_common", obj_elf_tls_common, 0},

  /* End sentinel.  */
  {NULL, NULL, 0},
};

#undef NO_RELOC

/* This is called when the assembler starts.  */

asection *elf_com_section_ptr;

void
elf_begin (void)
{
  asection *s;

  /* Add symbols for the known sections to the symbol table.  */
  s = bfd_get_section_by_name (stdoutput, TEXT_SECTION_NAME);
  symbol_table_insert (section_symbol (s));
  s = bfd_get_section_by_name (stdoutput, DATA_SECTION_NAME);
  symbol_table_insert (section_symbol (s));
  s = bfd_get_section_by_name (stdoutput, BSS_SECTION_NAME);
  symbol_table_insert (section_symbol (s));
  elf_com_section_ptr = bfd_com_section_ptr;
}

void
elf_pop_insert (void)
{
  pop_insert (elf_pseudo_table);
}

static bfd_vma
elf_s_get_size (symbolS *sym)
{
  return S_GET_SIZE (sym);
}

static void
elf_s_set_size (symbolS *sym, bfd_vma sz)
{
  S_SET_SIZE (sym, sz);
}

static bfd_vma
elf_s_get_align (symbolS *sym)
{
  return S_GET_ALIGN (sym);
}

static void
elf_s_set_align (symbolS *sym, bfd_vma align)
{
  S_SET_ALIGN (sym, align);
}

int
elf_s_get_other (symbolS *sym)
{
  return elf_symbol (symbol_get_bfdsym (sym))->internal_elf_sym.st_other;
}

static void
elf_s_set_other (symbolS *sym, int other)
{
  S_SET_OTHER (sym, other);
}

static int
elf_sec_sym_ok_for_reloc (asection *sec)
{
  return obj_sec_sym_ok_for_reloc (sec);
}

void
elf_file_symbol (const char *s, int appfile)
{
  if (!appfile
      || symbol_rootP == NULL
      || symbol_rootP->bsym == NULL
      || (symbol_rootP->bsym->flags & BSF_FILE) == 0)
    {
      symbolS *sym;
      size_t name_length;

      sym = symbol_new (s, absolute_section, 0, NULL);
      symbol_set_frag (sym, &zero_address_frag);

      name_length = strlen (s);
      if (name_length > strlen (S_GET_NAME (sym)))
	{
	  obstack_grow (&notes, s, name_length + 1);
	  S_SET_NAME (sym, (const char *) obstack_finish (&notes));
	}
      else
	strcpy ((char *) S_GET_NAME (sym), s);

      symbol_get_bfdsym (sym)->flags |= BSF_FILE;

      if (symbol_rootP != sym
	  && (symbol_rootP->bsym == NULL
	      || !(symbol_rootP->bsym->flags & BSF_FILE)))
	{
	  symbol_remove (sym, &symbol_rootP, &symbol_lastP);
	  symbol_insert (sym, symbol_rootP, &symbol_rootP, &symbol_lastP);
	}
    }
}

/* Called from read.c:s_comm after we've parsed .comm symbol, size.
   Parse a possible alignment value.  */

symbolS *
elf_common_parse (int ignore ATTRIBUTE_UNUSED, symbolS *symbolP, addressT size)
{
  addressT align = 0;
  int is_local = symbol_get_obj (symbolP)->local;

  if (*input_line_pointer == ',')
    {
      char *save = input_line_pointer;

      input_line_pointer++;
      SKIP_WHITESPACE ();

      if (*input_line_pointer == '"')
	{
	  /* For sparc.  Accept .common symbol, length, "bss"  */
	  input_line_pointer++;
	  /* Some use the dot, some don't.  */
	  if (*input_line_pointer == '.')
	    input_line_pointer++;
	  /* Some say data, some say bss.  */
	  if (strncmp (input_line_pointer, "bss\"", 4) == 0)
	    input_line_pointer += 4;
	  else if (strncmp (input_line_pointer, "data\"", 5) == 0)
	    input_line_pointer += 5;
	  else
	    {
	      char *p = input_line_pointer;
	      char c;

	      while (*--p != '"')
		;
	      while (!is_end_of_line[(unsigned char) *input_line_pointer])
		if (*input_line_pointer++ == '"')
		  break;
	      c = *input_line_pointer;
	      *input_line_pointer = '\0';
	      as_bad (_("bad .common segment %s"), p);
	      *input_line_pointer = c;
	      ignore_rest_of_line ();
	      return NULL;
	    }
	  /* ??? Don't ask me why these are always global.  */
	  is_local = 0;
	}
      else
	{
	  input_line_pointer = save;
	  align = parse_align (is_local);
	  if (align == (addressT) -1)
	    return NULL;
	}
    }

  if (is_local)
    {
      bss_alloc (symbolP, size, align);
      S_CLEAR_EXTERNAL (symbolP);
    }
  else
    {
      S_SET_VALUE (symbolP, size);
      S_SET_ALIGN (symbolP, align);
      S_SET_EXTERNAL (symbolP);
      S_SET_SEGMENT (symbolP, elf_com_section_ptr);
    }

  symbol_get_bfdsym (symbolP)->flags |= BSF_OBJECT;

  return symbolP;
}

void
obj_elf_common (int is_common)
{
  if (flag_mri && is_common)
    s_mri_common (0);
  else
    s_comm_internal (0, elf_common_parse);
}

static void
obj_elf_tls_common (int ignore ATTRIBUTE_UNUSED)
{
  symbolS *symbolP = s_comm_internal (0, elf_common_parse);

  if (symbolP)
    symbol_get_bfdsym (symbolP)->flags |= BSF_THREAD_LOCAL;
}

static void
obj_elf_lcomm (int ignore ATTRIBUTE_UNUSED)
{
  symbolS *symbolP = s_comm_internal (0, s_lcomm_internal);

  if (symbolP)
    symbol_get_bfdsym (symbolP)->flags |= BSF_OBJECT;
}

static symbolS *
get_sym_from_input_line_and_check (void)
{
  char *name;
  char c;
  symbolS *sym;

  c = get_symbol_name (& name);
  sym = symbol_find_or_make (name);
  *input_line_pointer = c;
  SKIP_WHITESPACE_AFTER_NAME ();

  /* There is no symbol name if input_line_pointer has not moved.  */
  if (name == input_line_pointer)
    as_bad (_("Missing symbol name in directive"));
  return sym;
}

static void
obj_elf_local (int ignore ATTRIBUTE_UNUSED)
{
  int c;
  symbolS *symbolP;

  do
    {
      symbolP = get_sym_from_input_line_and_check ();
      c = *input_line_pointer;
      S_CLEAR_EXTERNAL (symbolP);
      symbol_get_obj (symbolP)->local = 1;
      if (c == ',')
	{
	  input_line_pointer++;
	  SKIP_WHITESPACE ();
	  if (*input_line_pointer == '\n')
	    c = '\n';
	}
    }
  while (c == ',');
  demand_empty_rest_of_line ();
}

static void
obj_elf_weak (int ignore ATTRIBUTE_UNUSED)
{
  int c;
  symbolS *symbolP;

  do
    {
      symbolP = get_sym_from_input_line_and_check ();
      c = *input_line_pointer;
      S_SET_WEAK (symbolP);
      if (c == ',')
	{
	  input_line_pointer++;
	  SKIP_WHITESPACE ();
	  if (*input_line_pointer == '\n')
	    c = '\n';
	}
    }
  while (c == ',');
  demand_empty_rest_of_line ();
}

static void
obj_elf_visibility (int visibility)
{
  int c;
  symbolS *symbolP;
  asymbol *bfdsym;
  elf_symbol_type *elfsym;

  do
    {
      symbolP = get_sym_from_input_line_and_check ();

      bfdsym = symbol_get_bfdsym (symbolP);
      elfsym = elf_symbol_from (bfd_asymbol_bfd (bfdsym), bfdsym);

      gas_assert (elfsym);

      elfsym->internal_elf_sym.st_other &= ~3;
      elfsym->internal_elf_sym.st_other |= visibility;

      c = *input_line_pointer;
      if (c == ',')
	{
	  input_line_pointer ++;

	  SKIP_WHITESPACE ();

	  if (*input_line_pointer == '\n')
	    c = '\n';
	}
    }
  while (c == ',');

  demand_empty_rest_of_line ();
}

static segT previous_section;
static int previous_subsection;

struct section_stack
{
  struct section_stack *next;
  segT seg, prev_seg;
  int subseg, prev_subseg;
};

static struct section_stack *section_stack;

/* Match both section group name and the sh_info field.  */
struct section_match
{
  const char *group_name;
  unsigned int info;
};

static bfd_boolean
get_section (bfd *abfd ATTRIBUTE_UNUSED, asection *sec, void *inf)
{
  struct section_match *match = (struct section_match *) inf;
  const char *gname = match->group_name;
  const char *group_name = elf_group_name (sec);
  unsigned int info = elf_section_data (sec)->this_hdr.sh_info;

  return (info == match->info
	  && (group_name == gname
	      || (group_name != NULL
		  && gname != NULL
		  && strcmp (group_name, gname) == 0)));
}

/* Handle the .section pseudo-op.  This code supports two different
   syntaxes.

   The first is found on Solaris, and looks like
       .section ".sec1",#alloc,#execinstr,#write
   Here the names after '#' are the SHF_* flags to turn on for the
   section.  I'm not sure how it determines the SHT_* type (BFD
   doesn't really give us control over the type, anyhow).

   The second format is found on UnixWare, and probably most SVR4
   machines, and looks like
       .section .sec1,"a",@progbits
   The quoted string may contain any combination of a, w, x, and
   represents the SHF_* flags to turn on for the section.  The string
   beginning with '@' can be progbits or nobits.  There should be
   other possibilities, but I don't know what they are.  In any case,
   BFD doesn't really let us set the section type.  */

void
obj_elf_change_section (const char *name,
			unsigned int type,
			unsigned int info,
			bfd_vma attr,
			int entsize,
			const char *group_name,
			int linkonce,
			int push)
{
  asection *old_sec;
  segT sec;
  flagword flags;
  const struct elf_backend_data *bed;
  const struct bfd_elf_special_section *ssect;
  struct section_match match;

#ifdef md_flush_pending_output
  md_flush_pending_output ();
#endif

  /* Switch to the section, creating it if necessary.  */
  if (push)
    {
      struct section_stack *elt;
      elt = XNEW (struct section_stack);
      elt->next = section_stack;
      elt->seg = now_seg;
      elt->prev_seg = previous_section;
      elt->subseg = now_subseg;
      elt->prev_subseg = previous_subsection;
      section_stack = elt;
    }
  previous_section = now_seg;
  previous_subsection = now_subseg;

  match.group_name = group_name;
  match.info = info;
  old_sec = bfd_get_section_by_name_if (stdoutput, name, get_section,
					(void *) &match);
  if (old_sec)
    {
      sec = old_sec;
      subseg_set (sec, 0);
    }
  else
    sec = subseg_force_new (name, 0);

  bed = get_elf_backend_data (stdoutput);
  ssect = (*bed->get_sec_type_attr) (stdoutput, sec);

  if (ssect != NULL)
    {
      bfd_boolean override = FALSE;

      if (type == SHT_NULL)
	type = ssect->type;
      else if (type != ssect->type)
	{
	  if (old_sec == NULL
	      /* Some older versions of gcc will emit

		 .section .init_array,"aw",@progbits

		 for __attribute__ ((section (".init_array"))).
		 "@progbits" is incorrect.  Also for x86-64 large bss
		 sections, some older versions of gcc will emit

		 .section .lbss,"aw",@progbits

		 "@progbits" is incorrect.  */
	      && ssect->type != SHT_INIT_ARRAY
	      && ssect->type != SHT_FINI_ARRAY
	      && ssect->type != SHT_PREINIT_ARRAY)
	    {
	      /* We allow to specify any type for a .note section.  */
	      if (ssect->type != SHT_NOTE
		  /* Processor and application defined types are allowed too.  */
		  && type < SHT_LOPROC)
		as_warn (_("setting incorrect section type for %s"),
			 name);
	    }
	  else
	    {
	      as_warn (_("ignoring incorrect section type for %s"),
		       name);
	      type = ssect->type;
	    }
	}

      if (old_sec == NULL && ((attr & ~(SHF_MASKOS | SHF_MASKPROC))
			      & ~ssect->attr) != 0)
	{
	  /* As a GNU extension, we permit a .note section to be
	     allocatable.  If the linker sees an allocatable .note
	     section, it will create a PT_NOTE segment in the output
	     file.  We also allow "x" for .note.GNU-stack.  */
	  if (ssect->type == SHT_NOTE
	      && (attr == SHF_ALLOC || attr == SHF_EXECINSTR))
	    ;
	  /* Allow different SHF_MERGE and SHF_STRINGS if we have
	     something like .rodata.str.  */
	  else if (ssect->suffix_length == -2
		   && name[ssect->prefix_length] == '.'
		   && (attr
		       & ~ssect->attr
		       & ~SHF_MERGE
		       & ~SHF_STRINGS) == 0)
	    ;
	  /* .interp, .strtab and .symtab can have SHF_ALLOC.  */
	  else if (attr == SHF_ALLOC
		   && (strcmp (name, ".interp") == 0
		       || strcmp (name, ".strtab") == 0
		       || strcmp (name, ".symtab") == 0))
	    override = TRUE;
	  /* .note.GNU-stack can have SHF_EXECINSTR.  */
	  else if (attr == SHF_EXECINSTR
		   && strcmp (name, ".note.GNU-stack") == 0)
	    override = TRUE;
	  else
	    {
	      if (group_name == NULL)
		as_warn (_("setting incorrect section attributes for %s"),
			 name);
	      override = TRUE;
	    }
	}

      if (!override && old_sec == NULL)
	attr |= ssect->attr;
    }

  if ((attr & (SHF_ALLOC | SHF_GNU_MBIND)) == SHF_GNU_MBIND)
    as_fatal (_("SHF_ALLOC isn't set for GNU_MBIND section: %s"), name);

  /* Convert ELF type and flags to BFD flags.  */
  flags = (SEC_RELOC
	   | ((attr & SHF_WRITE) ? 0 : SEC_READONLY)
	   | ((attr & SHF_ALLOC) ? SEC_ALLOC : 0)
	   | (((attr & SHF_ALLOC) && type != SHT_NOBITS) ? SEC_LOAD : 0)
	   | ((attr & SHF_EXECINSTR) ? SEC_CODE : 0)
	   | ((attr & SHF_MERGE) ? SEC_MERGE : 0)
	   | ((attr & SHF_STRINGS) ? SEC_STRINGS : 0)
	   | ((attr & SHF_EXCLUDE) ? SEC_EXCLUDE: 0)
	   | ((attr & SHF_TLS) ? SEC_THREAD_LOCAL : 0));
#ifdef md_elf_section_flags
  flags = md_elf_section_flags (flags, attr, type);
#endif

  if (linkonce)
    flags |= SEC_LINK_ONCE | SEC_LINK_DUPLICATES_DISCARD;

  if (old_sec == NULL)
    {
      symbolS *secsym;

      if (type == SHT_NULL)
	type = bfd_elf_get_default_section_type (flags);
      elf_section_type (sec) = type;
      elf_section_flags (sec) = attr;
      elf_section_data (sec)->this_hdr.sh_info = info;

      /* Prevent SEC_HAS_CONTENTS from being inadvertently set.  */
      if (type == SHT_NOBITS)
	seg_info (sec)->bss = 1;

      bfd_set_section_flags (stdoutput, sec, flags);
      if (flags & SEC_MERGE)
	sec->entsize = entsize;
      elf_group_name (sec) = group_name;

      /* Add a symbol for this section to the symbol table.  */
      secsym = symbol_find (name);
      if (secsym != NULL)
	symbol_set_bfdsym (secsym, sec->symbol);
      else
	symbol_table_insert (section_symbol (sec));
    }
  else
    {
      if (type != SHT_NULL
	  && (unsigned) type != elf_section_type (old_sec))
	as_warn (_("ignoring changed section type for %s"), name);

      if (attr != 0)
	{
	  /* If section attributes are specified the second time we see a
	     particular section, then check that they are the same as we
	     saw the first time.  */
	  if (((old_sec->flags ^ flags)
	       & (SEC_ALLOC | SEC_LOAD | SEC_READONLY | SEC_CODE
		  | SEC_EXCLUDE | SEC_SORT_ENTRIES | SEC_MERGE | SEC_STRINGS
		  | SEC_LINK_ONCE | SEC_LINK_DUPLICATES_DISCARD
		  | SEC_THREAD_LOCAL)))
	    as_warn (_("ignoring changed section attributes for %s"), name);
	  else
	    /* FIXME: Maybe we should consider removing a previously set
	       processor or application specific attribute as suspicious ?  */
	    elf_section_flags (sec) = attr;

	  if ((flags & SEC_MERGE) && old_sec->entsize != (unsigned) entsize)
	    as_warn (_("ignoring changed section entity size for %s"), name);
	}
    }

#ifdef md_elf_section_change_hook
  md_elf_section_change_hook ();
#endif
}

static bfd_vma
obj_elf_parse_section_letters (char *str, size_t len, bfd_boolean *is_clone)
{
  bfd_vma attr = 0;
  *is_clone = FALSE;

  while (len > 0)
    {
      switch (*str)
	{
	case 'a':
	  attr |= SHF_ALLOC;
	  break;
	case 'e':
	  attr |= SHF_EXCLUDE;
	  break;
	case 'w':
	  attr |= SHF_WRITE;
	  break;
	case 'x':
	  attr |= SHF_EXECINSTR;
	  break;
	case 'M':
	  attr |= SHF_MERGE;
	  break;
	case 'S':
	  attr |= SHF_STRINGS;
	  break;
	case 'G':
	  attr |= SHF_GROUP;
	  break;
	case 'T':
	  attr |= SHF_TLS;
	  break;
	case 'd':
	  attr |= SHF_GNU_MBIND;
	  break;
	case '?':
	  *is_clone = TRUE;
	  break;
	/* Compatibility.  */
	case 'm':
	  if (*(str - 1) == 'a')
	    {
	      attr |= SHF_MERGE;
	      if (len > 1 && str[1] == 's')
		{
		  attr |= SHF_STRINGS;
		  str++, len--;
		}
	      break;
	    }
	  /* Fall through.  */
	default:
	  {
	    const char *bad_msg = _("unrecognized .section attribute:"
				    " want a,e,w,x,M,S,G,T or number");
#ifdef md_elf_section_letter
	    bfd_vma md_attr = md_elf_section_letter (*str, &bad_msg);
	    if (md_attr != (bfd_vma) -1)
	      attr |= md_attr;
	    else
#endif
	      if (ISDIGIT (*str))
		{
		  char * end;

		  attr |= strtoul (str, & end, 0);
		  /* Update str and len, allowing for the fact that
		     we will execute str++ and len-- below.  */
		  end --;
		  len -= (end - str);
		  str = end;
		}
	      else
		as_fatal ("%s", bad_msg);
	  }
	  break;
	}
      str++, len--;
    }

  return attr;
}

static int
obj_elf_section_type (char *str, size_t len, bfd_boolean warn)
{
  if (len == 8 && strncmp (str, "progbits", 8) == 0)
    return SHT_PROGBITS;
  if (len == 6 && strncmp (str, "nobits", 6) == 0)
    return SHT_NOBITS;
  if (len == 4 && strncmp (str, "note", 4) == 0)
    return SHT_NOTE;
  if (len == 10 && strncmp (str, "init_array", 10) == 0)
    return SHT_INIT_ARRAY;
  if (len == 10 && strncmp (str, "fini_array", 10) == 0)
    return SHT_FINI_ARRAY;
  if (len == 13 && strncmp (str, "preinit_array", 13) == 0)
    return SHT_PREINIT_ARRAY;

#ifdef md_elf_section_type
  {
    int md_type = md_elf_section_type (str, len);
    if (md_type >= 0)
      return md_type;
  }
#endif

  if (ISDIGIT (*str))
    {
      char * end;
      int type = strtoul (str, & end, 0);

      if (warn && (size_t) (end - str) != len)
	as_warn (_("extraneous characters at end of numeric section type"));

      return type;
    }

  if (warn)
    as_warn (_("unrecognized section type"));
  return 0;
}

static bfd_vma
obj_elf_section_word (char *str, size_t len, int *type)
{
  int ret;

  if (len == 5 && strncmp (str, "write", 5) == 0)
    return SHF_WRITE;
  if (len == 5 && strncmp (str, "alloc", 5) == 0)
    return SHF_ALLOC;
  if (len == 9 && strncmp (str, "execinstr", 9) == 0)
    return SHF_EXECINSTR;
  if (len == 7 && strncmp (str, "exclude", 7) == 0)
    return SHF_EXCLUDE;
  if (len == 3 && strncmp (str, "tls", 3) == 0)
    return SHF_TLS;

#ifdef md_elf_section_word
  {
    bfd_vma md_attr = md_elf_section_word (str, len);
    if (md_attr > 0)
      return md_attr;
  }
#endif

  ret = obj_elf_section_type (str, len, FALSE);
  if (ret != 0)
    *type = ret;
  else
    as_warn (_("unrecognized section attribute"));

  return 0;
}

/* Get name of section.  */
const char *
obj_elf_section_name (void)
{
  char *name;

  SKIP_WHITESPACE ();
  if (*input_line_pointer == '"')
    {
      int dummy;

      name = demand_copy_C_string (&dummy);
      if (name == NULL)
	{
	  ignore_rest_of_line ();
	  return NULL;
	}
    }
  else
    {
      char *end = input_line_pointer;

      while (0 == strchr ("\n\t,; ", *end))
	end++;
      if (end == input_line_pointer)
	{
	  as_bad (_("missing name"));
	  ignore_rest_of_line ();
	  return NULL;
	}

      name = xmemdup0 (input_line_pointer, end - input_line_pointer);

      while (flag_sectname_subst)
        {
	  char *subst = strchr (name, '%');
	  if (subst && subst[1] == 'S')
	    {
	      int oldlen = strlen (name);
	      int substlen = strlen (now_seg->name);
	      int newlen = oldlen - 2 + substlen;
	      char *newname = XNEWVEC (char, newlen + 1);
	      int headlen = subst - name;
	      memcpy (newname, name, headlen);
	      strcpy (newname + headlen, now_seg->name);
	      strcat (newname + headlen, subst + 2);
	      free (name);
	      name = newname;
	    }
	  else
	    break;
	}

#ifdef tc_canonicalize_section_name
      name = tc_canonicalize_section_name (name);
#endif
      input_line_pointer = end;
    }
  SKIP_WHITESPACE ();
  return name;
}

void
obj_elf_section (int push)
{
  const char *name, *group_name;
  char *beg;
  int type, dummy;
  bfd_vma attr;
  int entsize;
  int linkonce;
  subsegT new_subsection = -1;
  unsigned int info = 0;

  if (flag_mri)
    {
      char mri_type;

#ifdef md_flush_pending_output
      md_flush_pending_output ();
#endif

      previous_section = now_seg;
      previous_subsection = now_subseg;

      s_mri_sect (&mri_type);

#ifdef md_elf_section_change_hook
      md_elf_section_change_hook ();
#endif

      return;
    }

  name = obj_elf_section_name ();
  if (name == NULL)
    return;
  type = SHT_NULL;
  attr = 0;
  group_name = NULL;
  entsize = 0;
  linkonce = 0;

  if (*input_line_pointer == ',')
    {
      /* Skip the comma.  */
      ++input_line_pointer;
      SKIP_WHITESPACE ();

      if (push && ISDIGIT (*input_line_pointer))
	{
	  /* .pushsection has an optional subsection.  */
	  new_subsection = (subsegT) get_absolute_expression ();

	  SKIP_WHITESPACE ();

	  /* Stop if we don't see a comma.  */
	  if (*input_line_pointer != ',')
	    goto done;

	  /* Skip the comma.  */
	  ++input_line_pointer;
	  SKIP_WHITESPACE ();
	}

      if (*input_line_pointer == '"')
	{
	  bfd_boolean is_clone;

	  beg = demand_copy_C_string (&dummy);
	  if (beg == NULL)
	    {
	      ignore_rest_of_line ();
	      return;
	    }
	  attr |= obj_elf_parse_section_letters (beg, strlen (beg), &is_clone);

	  SKIP_WHITESPACE ();
	  if (*input_line_pointer == ',')
	    {
	      char c;
	      char *save = input_line_pointer;

	      ++input_line_pointer;
	      SKIP_WHITESPACE ();
	      c = *input_line_pointer;
	      if (c == '"')
		{
		  beg = demand_copy_C_string (&dummy);
		  if (beg == NULL)
		    {
		      ignore_rest_of_line ();
		      return;
		    }
		  type = obj_elf_section_type (beg, strlen (beg), TRUE);
		}
	      else if (c == '@' || c == '%')
		{
		  ++input_line_pointer;

		  if (ISDIGIT (* input_line_pointer))
		    {
		      type = strtoul (input_line_pointer, & input_line_pointer, 0);
		    }
		  else
		    {
		      c = get_symbol_name (& beg);
		      (void) restore_line_pointer (c);
		      type = obj_elf_section_type (beg, input_line_pointer - beg, TRUE);
		    }
		}
	      else
		input_line_pointer = save;
	    }

	  SKIP_WHITESPACE ();
	  if ((attr & SHF_MERGE) != 0 && *input_line_pointer == ',')
	    {
	      ++input_line_pointer;
	      SKIP_WHITESPACE ();
	      entsize = get_absolute_expression ();
	      SKIP_WHITESPACE ();
	      if (entsize < 0)
		{
		  as_warn (_("invalid merge entity size"));
		  attr &= ~SHF_MERGE;
		  entsize = 0;
		}
	    }
	  else if ((attr & SHF_MERGE) != 0)
	    {
	      as_warn (_("entity size for SHF_MERGE not specified"));
	      attr &= ~SHF_MERGE;
	    }

	  if ((attr & SHF_GROUP) != 0 && is_clone)
	    {
	      as_warn (_("? section flag ignored with G present"));
	      is_clone = FALSE;
	    }
	  if ((attr & SHF_GROUP) != 0 && *input_line_pointer == ',')
	    {
	      ++input_line_pointer;
	      group_name = obj_elf_section_name ();
	      if (group_name == NULL)
		attr &= ~SHF_GROUP;
	      else if (*input_line_pointer == ',')
		{
		  ++input_line_pointer;
		  SKIP_WHITESPACE ();
		  if (strncmp (input_line_pointer, "comdat", 6) == 0)
		    {
		      input_line_pointer += 6;
		      linkonce = 1;
		    }
		}
	      else if (strncmp (name, ".gnu.linkonce", 13) == 0)
		linkonce = 1;
	    }
	  else if ((attr & SHF_GROUP) != 0)
	    {
	      as_warn (_("group name for SHF_GROUP not specified"));
	      attr &= ~SHF_GROUP;
	    }

	  if (is_clone)
	    {
	      const char *now_group = elf_group_name (now_seg);
	      if (now_group != NULL)
		{
		  group_name = strdup (now_group);
		  linkonce = (now_seg->flags & SEC_LINK_ONCE) != 0;
		}
	    }

	  if ((attr & SHF_GNU_MBIND) != 0 && *input_line_pointer == ',')
	    {
	      ++input_line_pointer;
	      SKIP_WHITESPACE ();
	      if (ISDIGIT (* input_line_pointer))
		{
		  char *t = input_line_pointer;
		  info = strtoul (input_line_pointer,
				  &input_line_pointer, 0);
		  if (info == (unsigned int) -1)
		    {
		      as_warn (_("unsupported mbind section info: %s"), t);
		      info = 0;
		    }
		}
	    }
	}
      else
	{
	  do
	    {
	      char c;

	      SKIP_WHITESPACE ();
	      if (*input_line_pointer != '#')
		{
		  as_bad (_("character following name is not '#'"));
		  ignore_rest_of_line ();
		  return;
		}
	      ++input_line_pointer;
	      c = get_symbol_name (& beg);
	      (void) restore_line_pointer (c);

	      attr |= obj_elf_section_word (beg, input_line_pointer - beg, & type);

	      SKIP_WHITESPACE ();
	    }
	  while (*input_line_pointer++ == ',');
	  --input_line_pointer;
	}
    }

done:
  demand_empty_rest_of_line ();

  obj_elf_change_section (name, type, info, attr, entsize, group_name,
			  linkonce, push);

  if (push && new_subsection != -1)
    subseg_set (now_seg, new_subsection);
}

/* Change to the .data section.  */

void
obj_elf_data (int i)
{
#ifdef md_flush_pending_output
  md_flush_pending_output ();
#endif

  previous_section = now_seg;
  previous_subsection = now_subseg;
  s_data (i);

#ifdef md_elf_section_change_hook
  md_elf_section_change_hook ();
#endif
}

/* Change to the .text section.  */

void
obj_elf_text (int i)
{
#ifdef md_flush_pending_output
  md_flush_pending_output ();
#endif

  previous_section = now_seg;
  previous_subsection = now_subseg;
  s_text (i);

#ifdef md_elf_section_change_hook
  md_elf_section_change_hook ();
#endif
}

/* Change to the *ABS* section.  */

void
obj_elf_struct (int i)
{
#ifdef md_flush_pending_output
  md_flush_pending_output ();
#endif

  previous_section = now_seg;
  previous_subsection = now_subseg;
  s_struct (i);

#ifdef md_elf_section_change_hook
  md_elf_section_change_hook ();
#endif
}

static void
obj_elf_subsection (int ignore ATTRIBUTE_UNUSED)
{
  int temp;

#ifdef md_flush_pending_output
  md_flush_pending_output ();
#endif

  previous_section = now_seg;
  previous_subsection = now_subseg;

  temp = get_absolute_expression ();
  subseg_set (now_seg, (subsegT) temp);
  demand_empty_rest_of_line ();

#ifdef md_elf_section_change_hook
  md_elf_section_change_hook ();
#endif
}

/* This can be called from the processor backends if they change
   sections.  */

void
obj_elf_section_change_hook (void)
{
  previous_section = now_seg;
  previous_subsection = now_subseg;
}

void
obj_elf_previous (int ignore ATTRIBUTE_UNUSED)
{
  segT new_section;
  int new_subsection;

  if (previous_section == 0)
    {
      as_warn (_(".previous without corresponding .section; ignored"));
      return;
    }

#ifdef md_flush_pending_output
  md_flush_pending_output ();
#endif

  new_section = previous_section;
  new_subsection = previous_subsection;
  previous_section = now_seg;
  previous_subsection = now_subseg;
  subseg_set (new_section, new_subsection);

#ifdef md_elf_section_change_hook
  md_elf_section_change_hook ();
#endif
}

static void
obj_elf_popsection (int xxx ATTRIBUTE_UNUSED)
{
  struct section_stack *top = section_stack;

  if (top == NULL)
    {
      as_warn (_(".popsection without corresponding .pushsection; ignored"));
      return;
    }

#ifdef md_flush_pending_output
  md_flush_pending_output ();
#endif

  section_stack = top->next;
  previous_section = top->prev_seg;
  previous_subsection = top->prev_subseg;
  subseg_set (top->seg, top->subseg);
  free (top);

#ifdef md_elf_section_change_hook
  md_elf_section_change_hook ();
#endif
}

static void
obj_elf_line (int ignore ATTRIBUTE_UNUSED)
{
  /* Assume delimiter is part of expression.  BSD4.2 as fails with
     delightful bug, so we are not being incompatible here.  */
  new_logical_line (NULL, get_absolute_expression ());
  demand_empty_rest_of_line ();
}

/* This handles the .symver pseudo-op, which is used to specify a
   symbol version.  The syntax is ``.symver NAME,SYMVERNAME''.
   SYMVERNAME may contain ELF_VER_CHR ('@') characters.  This
   pseudo-op causes the assembler to emit a symbol named SYMVERNAME
   with the same value as the symbol NAME.  */

static void
obj_elf_symver (int ignore ATTRIBUTE_UNUSED)
{
  char *name;
  char c;
  char old_lexat;
  symbolS *sym;

  sym = get_sym_from_input_line_and_check ();

  if (*input_line_pointer != ',')
    {
      as_bad (_("expected comma after name in .symver"));
      ignore_rest_of_line ();
      return;
    }

  ++input_line_pointer;
  SKIP_WHITESPACE ();

  /* Temporarily include '@' in symbol names.  */
  old_lexat = lex_type[(unsigned char) '@'];
  lex_type[(unsigned char) '@'] |= LEX_NAME;
  c = get_symbol_name (& name);
  lex_type[(unsigned char) '@'] = old_lexat;

  if (S_IS_COMMON (sym))
    {
      as_bad (_("`%s' can't be versioned to common symbol '%s'"),
	      name, S_GET_NAME (sym));
      ignore_rest_of_line ();
      return;
    }

  if (symbol_get_obj (sym)->versioned_name == NULL)
    {
      symbol_get_obj (sym)->versioned_name = strdup (name);

      (void) restore_line_pointer (c);

      if (strchr (symbol_get_obj (sym)->versioned_name,
		  ELF_VER_CHR) == NULL)
	{
	  as_bad (_("missing version name in `%s' for symbol `%s'"),
		  symbol_get_obj (sym)->versioned_name,
		  S_GET_NAME (sym));
	  ignore_rest_of_line ();
	  return;
	}
    }
  else
    {
      if (strcmp (symbol_get_obj (sym)->versioned_name, name))
	{
	  as_bad (_("multiple versions [`%s'|`%s'] for symbol `%s'"),
		  name, symbol_get_obj (sym)->versioned_name,
		  S_GET_NAME (sym));
	  ignore_rest_of_line ();
	  return;
	}

      (void) restore_line_pointer (c);
    }

  demand_empty_rest_of_line ();
}

/* This handles the .vtable_inherit pseudo-op, which is used to indicate
   to the linker the hierarchy in which a particular table resides.  The
   syntax is ".vtable_inherit CHILDNAME, PARENTNAME".  */

struct fix *
obj_elf_get_vtable_inherit (void)
{
  char *cname, *pname;
  symbolS *csym, *psym;
  char c, bad = 0;

  if (*input_line_pointer == '#')
    ++input_line_pointer;

  c = get_symbol_name (& cname);
  csym = symbol_find (cname);

  /* GCFIXME: should check that we don't have two .vtable_inherits for
     the same child symbol.  Also, we can currently only do this if the
     child symbol is already exists and is placed in a fragment.  */

  if (csym == NULL || symbol_get_frag (csym) == NULL)
    {
      as_bad (_("expected `%s' to have already been set for .vtable_inherit"),
	      cname);
      bad = 1;
    }

  *input_line_pointer = c;

  SKIP_WHITESPACE_AFTER_NAME ();
  if (*input_line_pointer != ',')
    {
      as_bad (_("expected comma after name in .vtable_inherit"));
      ignore_rest_of_line ();
      return NULL;
    }

  ++input_line_pointer;
  SKIP_WHITESPACE ();

  if (*input_line_pointer == '#')
    ++input_line_pointer;

  if (input_line_pointer[0] == '0'
      && (input_line_pointer[1] == '\0'
	  || ISSPACE (input_line_pointer[1])))
    {
      psym = section_symbol (absolute_section);
      ++input_line_pointer;
    }
  else
    {
      c = get_symbol_name (& pname);
      psym = symbol_find_or_make (pname);
      restore_line_pointer (c);
    }

  demand_empty_rest_of_line ();

  if (bad)
    return NULL;

  gas_assert (symbol_get_value_expression (csym)->X_op == O_constant);
  return fix_new (symbol_get_frag (csym),
		  symbol_get_value_expression (csym)->X_add_number,
		  0, psym, 0, 0, BFD_RELOC_VTABLE_INHERIT);
}

/* This is a version of obj_elf_get_vtable_inherit() that is
   suitable for use in struct _pseudo_type tables.  */

void
obj_elf_vtable_inherit (int ignore ATTRIBUTE_UNUSED)
{
  (void) obj_elf_get_vtable_inherit ();
}

/* This handles the .vtable_entry pseudo-op, which is used to indicate
   to the linker that a vtable slot was used.  The syntax is
   ".vtable_entry tablename, offset".  */

struct fix *
obj_elf_get_vtable_entry (void)
{
  symbolS *sym;
  offsetT offset;

  if (*input_line_pointer == '#')
    ++input_line_pointer;

  sym = get_sym_from_input_line_and_check ();
  if (*input_line_pointer != ',')
    {
      as_bad (_("expected comma after name in .vtable_entry"));
      ignore_rest_of_line ();
      return NULL;
    }

  ++input_line_pointer;
  if (*input_line_pointer == '#')
    ++input_line_pointer;

  offset = get_absolute_expression ();

  demand_empty_rest_of_line ();

  return fix_new (frag_now, frag_now_fix (), 0, sym, offset, 0,
		  BFD_RELOC_VTABLE_ENTRY);
}

/* This is a version of obj_elf_get_vtable_entry() that is
   suitable for use in struct _pseudo_type tables.  */

void
obj_elf_vtable_entry (int ignore ATTRIBUTE_UNUSED)
{
  (void) obj_elf_get_vtable_entry ();
}

#define skip_whitespace(str)  do { if (*(str) == ' ') ++(str); } while (0)

static inline int
skip_past_char (char ** str, char c)
{
  if (**str == c)
    {
      (*str)++;
      return 0;
    }
  else
    return -1;
}
#define skip_past_comma(str) skip_past_char (str, ',')

/* A list of attributes that have been explicitly set by the assembly code.
   VENDOR is the vendor id, BASE is the tag shifted right by the number
   of bits in MASK, and bit N of MASK is set if tag BASE+N has been set.  */
struct recorded_attribute_info {
  struct recorded_attribute_info *next;
  int vendor;
  unsigned int base;
  unsigned long mask;
};
static struct recorded_attribute_info *recorded_attributes;

/* Record that we have seen an explicit specification of attribute TAG
   for vendor VENDOR.  */

static void
record_attribute (int vendor, unsigned int tag)
{
  unsigned int base;
  unsigned long mask;
  struct recorded_attribute_info *rai;

  base = tag / (8 * sizeof (rai->mask));
  mask = 1UL << (tag % (8 * sizeof (rai->mask)));
  for (rai = recorded_attributes; rai; rai = rai->next)
    if (rai->vendor == vendor && rai->base == base)
      {
	rai->mask |= mask;
	return;
      }

  rai = XNEW (struct recorded_attribute_info);
  rai->next = recorded_attributes;
  rai->vendor = vendor;
  rai->base = base;
  rai->mask = mask;
  recorded_attributes = rai;
}

/* Parse an attribute directive for VENDOR.
   Returns the attribute number read, or zero on error.  */

int
obj_elf_vendor_attribute (int vendor)
{
  expressionS exp;
  int type;
  int tag;
  unsigned int i = 0;
  char *s = NULL;

  /* Read the first number or name.  */
  skip_whitespace (input_line_pointer);
  s = input_line_pointer;
  if (ISDIGIT (*input_line_pointer))
    {
      expression (& exp);
      if (exp.X_op != O_constant)
	goto bad;
      tag = exp.X_add_number;
    }
  else
    {
      char *name;

      /* A name may contain '_', but no other punctuation.  */
      for (; ISALNUM (*input_line_pointer) || *input_line_pointer == '_';
	   ++input_line_pointer)
	i++;
      if (i == 0)
	goto bad;

      name = xstrndup (s, i);

#ifndef CONVERT_SYMBOLIC_ATTRIBUTE
#define CONVERT_SYMBOLIC_ATTRIBUTE(a) -1
#endif

      tag = CONVERT_SYMBOLIC_ATTRIBUTE (name);
      if (tag == -1)
	{
	  as_bad (_("Attribute name not recognised: %s"), name);
	  ignore_rest_of_line ();
	  free (name);
	  return 0;
	}
      free (name);
    }

  type = _bfd_elf_obj_attrs_arg_type (stdoutput, vendor, tag);

  if (skip_past_comma (&input_line_pointer) == -1)
    goto bad;
  if (type & 1)
    {
      expression (& exp);
      if (exp.X_op != O_constant)
	{
	  as_bad (_("expected numeric constant"));
	  ignore_rest_of_line ();
	  return 0;
	}
      i = exp.X_add_number;
    }
  if ((type & 3) == 3
      && skip_past_comma (&input_line_pointer) == -1)
    {
      as_bad (_("expected comma"));
      ignore_rest_of_line ();
      return 0;
    }
  if (type & 2)
    {
      int len;

      skip_whitespace (input_line_pointer);
      if (*input_line_pointer != '"')
	goto bad_string;
      s = demand_copy_C_string (&len);
    }

  record_attribute (vendor, tag);
  switch (type & 3)
    {
    case 3:
      bfd_elf_add_obj_attr_int_string (stdoutput, vendor, tag, i, s);
      break;
    case 2:
      bfd_elf_add_obj_attr_string (stdoutput, vendor, tag, s);
      break;
    case 1:
      bfd_elf_add_obj_attr_int (stdoutput, vendor, tag, i);
      break;
    default:
      abort ();
    }

  demand_empty_rest_of_line ();
  return tag;
bad_string:
  as_bad (_("bad string constant"));
  ignore_rest_of_line ();
  return 0;
bad:
  as_bad (_("expected <tag> , <value>"));
  ignore_rest_of_line ();
  return 0;
}

/* Parse a .gnu_attribute directive.  */

static void
obj_elf_gnu_attribute (int ignored ATTRIBUTE_UNUSED)
{
  obj_elf_vendor_attribute (OBJ_ATTR_GNU);
}

void
elf_obj_read_begin_hook (void)
{
}

void
elf_obj_symbol_new_hook (symbolS *symbolP)
{
  struct elf_obj_sy *sy_obj;

  sy_obj = symbol_get_obj (symbolP);
  sy_obj->size = NULL;
  sy_obj->versioned_name = NULL;
}

/* When setting one symbol equal to another, by default we probably
   want them to have the same "size", whatever it means in the current
   context.  */

void
elf_copy_symbol_attributes (symbolS *dest, symbolS *src)
{
  struct elf_obj_sy *srcelf = symbol_get_obj (src);
  struct elf_obj_sy *destelf = symbol_get_obj (dest);
  if (srcelf->size)
    {
      if (destelf->size == NULL)
	destelf->size = XNEW (expressionS);
      *destelf->size = *srcelf->size;
    }
  else
    {
      if (destelf->size != NULL)
	free (destelf->size);
      destelf->size = NULL;
    }
  S_SET_SIZE (dest, S_GET_SIZE (src));
  /* Don't copy visibility.  */
  S_SET_OTHER (dest, (ELF_ST_VISIBILITY (S_GET_OTHER (dest))
		      | (S_GET_OTHER (src) & ~ELF_ST_VISIBILITY (-1))));
}

void
obj_elf_version (int ignore ATTRIBUTE_UNUSED)
{
  char *name;
  unsigned int c;
  char *p;
  asection *seg = now_seg;
  subsegT subseg = now_subseg;
  Elf_Internal_Note i_note;
  Elf_External_Note e_note;
  asection *note_secp = NULL;

  SKIP_WHITESPACE ();
  if (*input_line_pointer == '\"')
    {
      unsigned int len;

      ++input_line_pointer;	/* -> 1st char of string.  */
      name = input_line_pointer;

      while (is_a_char (c = next_char_of_string ()))
	;
      c = *input_line_pointer;
      *input_line_pointer = '\0';
      *(input_line_pointer - 1) = '\0';
      *input_line_pointer = c;

      /* Create the .note section.  */
      note_secp = subseg_new (".note", 0);
      bfd_set_section_flags (stdoutput,
			     note_secp,
			     SEC_HAS_CONTENTS | SEC_READONLY);
      record_alignment (note_secp, 2);

      /* Process the version string.  */
      len = strlen (name) + 1;

      /* PR 3456: Although the name field is padded out to an 4-byte
	 boundary, the namesz field should not be adjusted.  */
      i_note.namesz = len;
      i_note.descsz = 0;	/* No description.  */
      i_note.type = NT_VERSION;
      p = frag_more (sizeof (e_note.namesz));
      md_number_to_chars (p, i_note.namesz, sizeof (e_note.namesz));
      p = frag_more (sizeof (e_note.descsz));
      md_number_to_chars (p, i_note.descsz, sizeof (e_note.descsz));
      p = frag_more (sizeof (e_note.type));
      md_number_to_chars (p, i_note.type, sizeof (e_note.type));
      p = frag_more (len);
      memcpy (p, name, len);

      frag_align (2, 0, 0);

      subseg_set (seg, subseg);
    }
  else
    as_bad (_("expected quoted string"));

  demand_empty_rest_of_line ();
}

static void
obj_elf_size (int ignore ATTRIBUTE_UNUSED)
{
  char *name;
  char c = get_symbol_name (&name);
  char *p;
  expressionS exp;
  symbolS *sym;

  p = input_line_pointer;
  *p = c;
  SKIP_WHITESPACE_AFTER_NAME ();
  if (*input_line_pointer != ',')
    {
      *p = 0;
      as_bad (_("expected comma after name `%s' in .size directive"), name);
      *p = c;
      ignore_rest_of_line ();
      return;
    }
  input_line_pointer++;
  expression (&exp);
  if (exp.X_op == O_absent)
    {
      as_bad (_("missing expression in .size directive"));
      exp.X_op = O_constant;
      exp.X_add_number = 0;
    }
  *p = 0;
  sym = symbol_find_or_make (name);
  *p = c;
  if (exp.X_op == O_constant)
    {
      S_SET_SIZE (sym, exp.X_add_number);
      if (symbol_get_obj (sym)->size)
	{
	  free (symbol_get_obj (sym)->size);
	  symbol_get_obj (sym)->size = NULL;
	}
    }
  else
    {
      symbol_get_obj (sym)->size = XNEW (expressionS);
      *symbol_get_obj (sym)->size = exp;
    }
  demand_empty_rest_of_line ();
}

/* Handle the ELF .type pseudo-op.  This sets the type of a symbol.
   There are six syntaxes:

   The first (used on Solaris) is
       .type SYM,#function
   The second (used on UnixWare) is
       .type SYM,@function
   The third (reportedly to be used on Irix 6.0) is
       .type SYM STT_FUNC
   The fourth (used on NetBSD/Arm and Linux/ARM) is
       .type SYM,%function
   The fifth (used on SVR4/860) is
       .type SYM,"function"
   The sixth (emitted by recent SunPRO under Solaris) is
       .type SYM,[0-9]
   where the integer is the STT_* value.
   */

static char *
obj_elf_type_name (char *cp)
{
  char *p;

  p = input_line_pointer;
  if (*input_line_pointer >= '0'
      && *input_line_pointer <= '9')
    {
      while (*input_line_pointer >= '0'
	     && *input_line_pointer <= '9')
	++input_line_pointer;
      *cp = *input_line_pointer;
      *input_line_pointer = '\0';
    }
  else
    *cp = get_symbol_name (&p);

  return p;
}

static void
obj_elf_type (int ignore ATTRIBUTE_UNUSED)
{
  char c;
  int type;
  const char *type_name;
  symbolS *sym;
  elf_symbol_type *elfsym;

  sym = get_sym_from_input_line_and_check ();
  c = *input_line_pointer;
  elfsym = (elf_symbol_type *) symbol_get_bfdsym (sym);

  if (*input_line_pointer == ',')
    ++input_line_pointer;

  SKIP_WHITESPACE ();
  if (   *input_line_pointer == '#'
      || *input_line_pointer == '@'
      || *input_line_pointer == '"'
      || *input_line_pointer == '%')
    ++input_line_pointer;

  type_name = obj_elf_type_name (& c);

  type = 0;
  if (strcmp (type_name, "function") == 0
      || strcmp (type_name, "2") == 0
      || strcmp (type_name, "STT_FUNC") == 0)
    type = BSF_FUNCTION;
  else if (strcmp (type_name, "object") == 0
	   || strcmp (type_name, "1") == 0
	   || strcmp (type_name, "STT_OBJECT") == 0)
    type = BSF_OBJECT;
  else if (strcmp (type_name, "tls_object") == 0
	   || strcmp (type_name, "6") == 0
	   || strcmp (type_name, "STT_TLS") == 0)
    type = BSF_OBJECT | BSF_THREAD_LOCAL;
  else if (strcmp (type_name, "notype") == 0
	   || strcmp (type_name, "0") == 0
	   || strcmp (type_name, "STT_NOTYPE") == 0)
    ;
  else if (strcmp (type_name, "common") == 0
	   || strcmp (type_name, "5") == 0
	   || strcmp (type_name, "STT_COMMON") == 0)
    {
      type = BSF_OBJECT;

      if (! S_IS_COMMON (sym))
	{
	  if (S_IS_VOLATILE (sym))
	    {
	      sym = symbol_clone (sym, 1);
	      S_SET_SEGMENT (sym, bfd_com_section_ptr);
	      S_SET_VALUE (sym, 0);
	      S_SET_EXTERNAL (sym);
	      symbol_set_frag (sym, &zero_address_frag);
	      S_CLEAR_VOLATILE (sym);
	    }
	  else if (S_IS_DEFINED (sym) || symbol_equated_p (sym))
	    as_bad (_("symbol '%s' is already defined"), S_GET_NAME (sym));
	  else
	    {
	      /* FIXME: Is it safe to just change the section ?  */
	      S_SET_SEGMENT (sym, bfd_com_section_ptr);
	      S_SET_VALUE (sym, 0);
	      S_SET_EXTERNAL (sym);
	    }
	}
    }
  else if (strcmp (type_name, "gnu_indirect_function") == 0
	   || strcmp (type_name, "10") == 0
	   || strcmp (type_name, "STT_GNU_IFUNC") == 0)
    {
      const struct elf_backend_data *bed;

      bed = get_elf_backend_data (stdoutput);
      if (!(bed->elf_osabi == ELFOSABI_GNU
	    || bed->elf_osabi == ELFOSABI_FREEBSD
	    /* GNU is still using the default value 0.  */
	    || bed->elf_osabi == ELFOSABI_NONE))
	as_bad (_("symbol type \"%s\" is supported only by GNU and FreeBSD targets"),
		type_name);
      type = BSF_FUNCTION | BSF_GNU_INDIRECT_FUNCTION;
    }
  else if (strcmp (type_name, "gnu_unique_object") == 0)
    {
      struct elf_backend_data *bed;

      bed = (struct elf_backend_data *) get_elf_backend_data (stdoutput);
      if (!(bed->elf_osabi == ELFOSABI_GNU
	    /* GNU is still using the default value 0.  */
	    || bed->elf_osabi == ELFOSABI_NONE))
	as_bad (_("symbol type \"%s\" is supported only by GNU targets"),
		type_name);
      type = BSF_OBJECT | BSF_GNU_UNIQUE;
      /* PR 10549: Always set OSABI field to GNU for objects containing unique symbols.  */
      bed->elf_osabi = ELFOSABI_GNU;
    }
#ifdef md_elf_symbol_type
  else if ((type = md_elf_symbol_type (type_name, sym, elfsym)) != -1)
    ;
#endif
  else
    as_bad (_("unrecognized symbol type \"%s\""), type_name);

  *input_line_pointer = c;

  if (*input_line_pointer == '"')
    ++input_line_pointer;

  elfsym->symbol.flags |= type;

  demand_empty_rest_of_line ();
}

static void
obj_elf_ident (int ignore ATTRIBUTE_UNUSED)
{
  static segT comment_section;
  segT old_section = now_seg;
  int old_subsection = now_subseg;

#ifdef md_flush_pending_output
  md_flush_pending_output ();
#endif

  if (!comment_section)
    {
      char *p;
      comment_section = subseg_new (".comment", 0);
      bfd_set_section_flags (stdoutput, comment_section,
			     SEC_READONLY | SEC_HAS_CONTENTS
			     | SEC_MERGE | SEC_STRINGS);
      comment_section->entsize = 1;
#ifdef md_elf_section_change_hook
      md_elf_section_change_hook ();
#endif
      p = frag_more (1);
      *p = 0;
    }
  else
    subseg_set (comment_section, 0);
  stringer (8 + 1);
  subseg_set (old_section, old_subsection);
}

void
elf_frob_symbol (symbolS *symp, int *puntp)
{
  struct elf_obj_sy *sy_obj;
  expressionS *size;

  sy_obj = symbol_get_obj (symp);

  size = sy_obj->size;
  if (size != NULL)
    {
      if (resolve_expression (size)
	  && size->X_op == O_constant)
	S_SET_SIZE (symp, size->X_add_number);
      else
	{
	  if (!flag_allow_nonconst_size)
	    as_bad (_(".size expression for %s "
		      "does not evaluate to a constant"), S_GET_NAME (symp));
	  else
	    as_warn (_(".size expression for %s "
		       "does not evaluate to a constant"), S_GET_NAME (symp));
	}
      free (sy_obj->size);
      sy_obj->size = NULL;
    }

  if (sy_obj->versioned_name != NULL)
    {
      char *p;

      p = strchr (sy_obj->versioned_name, ELF_VER_CHR);
      if (p == NULL)
	/* We will have already reported an error about a missing version.  */
	*puntp = TRUE;

      /* This symbol was given a new name with the .symver directive.

	 If this is an external reference, just rename the symbol to
	 include the version string.  This will make the relocs be
	 against the correct versioned symbol.

	 If this is a definition, add an alias.  FIXME: Using an alias
	 will permit the debugging information to refer to the right
	 symbol.  However, it's not clear whether it is the best
	 approach.  */

      else if (! S_IS_DEFINED (symp))
	{
	  /* Verify that the name isn't using the @@ syntax--this is
	     reserved for definitions of the default version to link
	     against.  */
	  if (p[1] == ELF_VER_CHR)
	    {
	      as_bad (_("invalid attempt to declare external version name"
			" as default in symbol `%s'"),
		      sy_obj->versioned_name);
	      *puntp = TRUE;
	    }
	  S_SET_NAME (symp, sy_obj->versioned_name);
	}
      else
	{
	  if (p[1] == ELF_VER_CHR && p[2] == ELF_VER_CHR)
	    {
	      size_t l;

	      /* The @@@ syntax is a special case. It renames the
		 symbol name to versioned_name with one `@' removed.  */
	      l = strlen (&p[3]) + 1;
	      memmove (&p[2], &p[3], l);
	      S_SET_NAME (symp, sy_obj->versioned_name);
	    }
	  else
	    {
	      symbolS *symp2;

	      /* FIXME: Creating a new symbol here is risky.  We're
		 in the final loop over the symbol table.  We can
		 get away with it only because the symbol goes to
		 the end of the list, where the loop will still see
		 it.  It would probably be better to do this in
		 obj_frob_file_before_adjust.  */

	      symp2 = symbol_find_or_make (sy_obj->versioned_name);

	      /* Now we act as though we saw symp2 = sym.  */
	      if (S_IS_COMMON (symp))
		{
		  as_bad (_("`%s' can't be versioned to common symbol '%s'"),
			  sy_obj->versioned_name, S_GET_NAME (symp));
		  *puntp = TRUE;
		  return;
		}

	      S_SET_SEGMENT (symp2, S_GET_SEGMENT (symp));

	      /* Subtracting out the frag address here is a hack
		 because we are in the middle of the final loop.  */
	      S_SET_VALUE (symp2,
			   (S_GET_VALUE (symp)
			    - symbol_get_frag (symp)->fr_address));

	      symbol_set_frag (symp2, symbol_get_frag (symp));

	      /* This will copy over the size information.  */
	      copy_symbol_attributes (symp2, symp);

	      S_SET_OTHER (symp2, S_GET_OTHER (symp));

	      if (S_IS_WEAK (symp))
		S_SET_WEAK (symp2);

	      if (S_IS_EXTERNAL (symp))
		S_SET_EXTERNAL (symp2);
	    }
	}
    }

  /* Double check weak symbols.  */
  if (S_IS_WEAK (symp))
    {
      if (S_IS_COMMON (symp))
	as_bad (_("symbol `%s' can not be both weak and common"),
		S_GET_NAME (symp));
    }
}

struct group_list
{
  asection **head;		/* Section lists.  */
  unsigned int num_group;	/* Number of lists.  */
  struct hash_control *indexes; /* Maps group name to index in head array.  */
};

static struct group_list groups;

/* Called via bfd_map_over_sections.  If SEC is a member of a group,
   add it to a list of sections belonging to the group.  INF is a
   pointer to a struct group_list, which is where we store the head of
   each list.  */

static void
build_group_lists (bfd *abfd ATTRIBUTE_UNUSED, asection *sec, void *inf)
{
  struct group_list *list = (struct group_list *) inf;
  const char *group_name = elf_group_name (sec);
  unsigned int i;
  unsigned int *elem_idx;
  unsigned int *idx_ptr;

  if (group_name == NULL)
    return;

  /* If this group already has a list, add the section to the head of
     the list.  */
  elem_idx = (unsigned int *) hash_find (list->indexes, group_name);
  if (elem_idx != NULL)
    {
      elf_next_in_group (sec) = list->head[*elem_idx];
      list->head[*elem_idx] = sec;
      return;
    }

  /* New group.  Make the arrays bigger in chunks to minimize calls to
     realloc.  */
  i = list->num_group;
  if ((i & 127) == 0)
    {
      unsigned int newsize = i + 128;
      list->head = XRESIZEVEC (asection *, list->head, newsize);
    }
  list->head[i] = sec;
  list->num_group += 1;

  /* Add index to hash.  */
  idx_ptr = XNEW (unsigned int);
  *idx_ptr = i;
  hash_insert (list->indexes, group_name, idx_ptr);
}

static void free_section_idx (const char *key ATTRIBUTE_UNUSED, void *val)
{
  free ((unsigned int *) val);
}

/* Create symbols for group signature.  */

void
elf_adjust_symtab (void)
{
  unsigned int i;

  /* Go find section groups.  */
  groups.num_group = 0;
  groups.head = NULL;
  groups.indexes = hash_new ();
  bfd_map_over_sections (stdoutput, build_group_lists, &groups);

  /* Make the SHT_GROUP sections that describe each section group.  We
     can't set up the section contents here yet, because elf section
     indices have yet to be calculated.  elf.c:set_group_contents does
     the rest of the work.  */
 for (i = 0; i < groups.num_group; i++)
    {
      const char *group_name = elf_group_name (groups.head[i]);
      const char *sec_name;
      asection *s;
      flagword flags;
      struct symbol *sy;

      flags = SEC_READONLY | SEC_HAS_CONTENTS | SEC_IN_MEMORY | SEC_GROUP;
      for (s = groups.head[i]; s != NULL; s = elf_next_in_group (s))
	if ((s->flags ^ flags) & SEC_LINK_ONCE)
	  {
	    flags |= SEC_LINK_ONCE | SEC_LINK_DUPLICATES_DISCARD;
	    if (s != groups.head[i])
	      {
		as_warn (_("assuming all members of group `%s' are COMDAT"),
			 group_name);
		break;
	      }
	  }

      sec_name = ".group";
      s = subseg_force_new (sec_name, 0);
      if (s == NULL
	  || !bfd_set_section_flags (stdoutput, s, flags)
	  || !bfd_set_section_alignment (stdoutput, s, 2))
	{
	  as_fatal (_("can't create group: %s"),
		    bfd_errmsg (bfd_get_error ()));
	}
      elf_section_type (s) = SHT_GROUP;

      /* Pass a pointer to the first section in this group.  */
      elf_next_in_group (s) = groups.head[i];
      elf_sec_group (groups.head[i]) = s;
      /* Make sure that the signature symbol for the group has the
	 name of the group.  */
      sy = symbol_find_exact (group_name);
      if (!sy
	  || (sy != symbol_lastP
	      && (sy->sy_flags.sy_local_symbol
		  || sy->sy_next == NULL
		  || sy->sy_next->sy_previous != sy)))
	{
	  /* Create the symbol now.  */
	  sy = symbol_new (group_name, now_seg, (valueT) 0, frag_now);
#ifdef TE_SOLARIS
	  /* Before Solaris 11 build 154, Sun ld rejects local group
	     signature symbols, so make them weak hidden instead.  */
	  symbol_get_bfdsym (sy)->flags |= BSF_WEAK;
	  S_SET_OTHER (sy, STV_HIDDEN);
#else
	  symbol_get_obj (sy)->local = 1;
#endif
	  symbol_table_insert (sy);
	}
      elf_group_id (s) = symbol_get_bfdsym (sy);
    }
}

void
elf_frob_file (void)
{
#ifdef elf_tc_final_processing
  elf_tc_final_processing ();
#endif
}

/* It removes any unneeded versioned symbols from the symbol table.  */

void
elf_frob_file_before_adjust (void)
{
  if (symbol_rootP)
    {
      symbolS *symp;

      for (symp = symbol_rootP; symp; symp = symbol_next (symp))
	if (!S_IS_DEFINED (symp))
	  {
	    if (symbol_get_obj (symp)->versioned_name)
	      {
		char *p;

		/* The @@@ syntax is a special case. If the symbol is
		   not defined, 2 `@'s will be removed from the
		   versioned_name.  */

		p = strchr (symbol_get_obj (symp)->versioned_name,
			    ELF_VER_CHR);
		if (p != NULL && p[1] == ELF_VER_CHR && p[2] == ELF_VER_CHR)
		  {
		    size_t l = strlen (&p[3]) + 1;
		    memmove (&p[1], &p[3], l);
		  }
		if (symbol_used_p (symp) == 0
		    && symbol_used_in_reloc_p (symp) == 0)
		  symbol_remove (symp, &symbol_rootP, &symbol_lastP);
	      }

	    /* If there was .weak foo, but foo was neither defined nor
	       used anywhere, remove it.  */

	    else if (S_IS_WEAK (symp)
		     && symbol_used_p (symp) == 0
		     && symbol_used_in_reloc_p (symp) == 0)
	      symbol_remove (symp, &symbol_rootP, &symbol_lastP);
	  }
    }
}

/* It is required that we let write_relocs have the opportunity to
   optimize away fixups before output has begun, since it is possible
   to eliminate all fixups for a section and thus we never should
   have generated the relocation section.  */

void
elf_frob_file_after_relocs (void)
{
  unsigned int i;

  /* Set SHT_GROUP section size.  */
  for (i = 0; i < groups.num_group; i++)
    {
      asection *s, *head, *group;
      bfd_size_type size;

      head = groups.head[i];
      size = 4;
      for (s = head; s != NULL; s = elf_next_in_group (s))
	size += (s->flags & SEC_RELOC) != 0 ? 8 : 4;

      group = elf_sec_group (head);
      subseg_set (group, 0);
      bfd_set_section_size (stdoutput, group, size);
      group->contents = (unsigned char *) frag_more (size);
      frag_now->fr_fix = frag_now_fix_octets ();
      frag_wane (frag_now);
    }

  /* Cleanup hash.  */
  hash_traverse (groups.indexes, free_section_idx);
  hash_die (groups.indexes);
}

static void
elf_generate_asm_lineno (void)
{
}

const struct format_ops elf_format_ops =
{
  bfd_target_elf_flavour,
  0,	/* dfl_leading_underscore */
  1,	/* emit_section_symbols */
  elf_begin,
  elf_file_symbol,
  elf_frob_symbol,
  elf_frob_file,
  elf_frob_file_before_adjust,
  0,	/* obj_frob_file_before_fix */
  elf_frob_file_after_relocs,
  elf_s_get_size, elf_s_set_size,
  elf_s_get_align, elf_s_set_align,
  elf_s_get_other,
  elf_s_set_other,
  0,	/* s_get_desc */
  0,	/* s_set_desc */
  0,	/* s_get_type */
  0,	/* s_set_type */
  elf_copy_symbol_attributes,
  elf_generate_asm_lineno,
  0,
  0,
  0,
  elf_sec_sym_ok_for_reloc,
  elf_pop_insert,
  0,	/* ecoff_set_ext */
  elf_obj_read_begin_hook,
  elf_obj_symbol_new_hook,
  0,
  elf_adjust_symtab
};
