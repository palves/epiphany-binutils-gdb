/* Adapteva epiphany specific support for 32-bit ELF
   Copyright (C) 2000-2015 Free Software Foundation, Inc.
   Contributed by Embecosm on behalf of Adapteva, Inc.

   This file is part of BFD, the Binary File Descriptor library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#include "sysdep.h"
#include "bfd.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/epiphany.h"
#include "libiberty.h"

#define PLT_ENTRY_SIZE 16

/* Current PLT Design:
   [....] Push LR to stack
   [....] Jump with Link to cachemanager
   [....] Startaddr
   [....] Size (or should this be endaddr?) */
   

/* Struct used to pass miscellaneous paramaters which
   helps to avoid overly long parameter lists.  */
struct misc
{
  Elf_Internal_Shdr *  symtab_hdr;
  Elf_Internal_Rela *  irelbase;
  bfd_byte *           contents;
  Elf_Internal_Sym *   isymbuf;
};

struct epiphany_opcode
{
  unsigned short opcode;
  unsigned short mask;
};

static bfd_boolean epiphany_relaxed = FALSE;

/* Relocation tables.  */
static reloc_howto_type epiphany_elf_howto_table [] =
{
#define AHOW(t,rs,s,bs,pr,bp,co,name,sm,dm)	\
    HOWTO(t,                    /* type */ \
	  rs,                   /* rightshift */ \
	  s,                    /* size (0 = byte, 1 = short, 2 = long) */ \
	  bs,                   /* bitsize */ \
	  pr,                   /* pc_relative */ \
	  bp,                   /* bitpos */ \
	  co,			/* complain_on_overflow */	       \
	  bfd_elf_generic_reloc,/* special_function */ \
	  name,                 /* name */ \
	  FALSE,                /* partial_inplace */ \
	  sm,                   /* src_mask */ \
	  dm,                   /* dst_mask */ \
	  pr)                   /* pcrel_offset */

  /* This reloc does nothing.  */
  AHOW (R_EPIPHANY_NONE,    0, 3,0, FALSE, 0, complain_overflow_dont,     "R_EPIPHANY_NONE",        0,          0),

  /* 8 bit absolute (not likely) */
  AHOW (R_EPIPHANY_8,       0, 0, 8, FALSE, 0, complain_overflow_bitfield, "R_EPIPHANY_8",      0x000000ff, 0x000000ff),
  /* 16 bit absolute */
  AHOW (R_EPIPHANY_16,      0, 1,16, FALSE, 0, complain_overflow_bitfield, "R_EPIPHANY_16",     0x0000ffff, 0x00ff1fe0),
  /* A 32 bit absolute relocation.  */
  AHOW (R_EPIPHANY_32,      0, 2,32, FALSE, 0, complain_overflow_dont,     "R_EPIPHANY_32",     0xffffffff, 0xffffffff),

  /*  8 bit relative relocation */
  HOWTO ( R_EPIPHANY_8_PCREL,  0, 0,  8, TRUE, 0, complain_overflow_bitfield, bfd_elf_generic_reloc, "R_EPIPHANY_8_PCREL", FALSE, 0x000000ff, 0x000000ff, FALSE),
  /* 16 bit relative relocation */
  HOWTO ( R_EPIPHANY_16_PCREL, 0, 1, 16, TRUE, 0, complain_overflow_bitfield, bfd_elf_generic_reloc, "R_EPIPHANY_8_PCREL", FALSE, 0x000000ff, 0x000000ff, FALSE),
  /* 32 bit relative relocation */
  HOWTO ( R_EPIPHANY_32_PCREL, 0, 2, 32, TRUE, 0, complain_overflow_bitfield, bfd_elf_generic_reloc, "R_EPIPHANY_8_PCREL", FALSE, 0x000000ff, 0x000000ff, FALSE),

  /* 8 bit pc-relative relocation */
  AHOW (R_EPIPHANY_SIMM8,   1, 0, 8,  TRUE, 8, complain_overflow_signed,   "R_EPIPHANY_SIMM8",   0x000000ff, 0x0000ff00),
  /* 24 bit pc-relative relocation */
  AHOW (R_EPIPHANY_SIMM24,  1, 2,24,  TRUE, 8, complain_overflow_signed,   "R_EPIPHANY_SIMM24",  0x00ffffff, 0xffffff00),

  /* %HIGH(EA) */
  AHOW (R_EPIPHANY_HIGH,    0, 2,16, FALSE, 0, complain_overflow_dont,     "R_EPIPHANY_HIGH",    0x0ff01fe0, 0x0ff01fe0),

  /* %LOW(EA) */
  AHOW (R_EPIPHANY_LOW,     0, 2,16, FALSE, 0, complain_overflow_dont,	"R_EPIPHANY_LOW",     0x0ff01fe0, 0x0ff01fe0),

  /* simm11 */
  AHOW (R_EPIPHANY_SIMM11,  0, 2,11, FALSE, 0, complain_overflow_bitfield, "R_EPIPHANY_SIMM11",  0x00ff0380, 0x00ff0380),
  /* imm12 - sign-magnitude */
  AHOW (R_EPIPHANY_IMM11,   0, 2,11, FALSE, 0, complain_overflow_bitfield, "R_EPIPHANY_IMM12",   0x00ff0380, 0x00ff0380),
  /* imm8 */
  AHOW (R_EPIPHANY_IMM8,    0, 1, 8, FALSE, 8, complain_overflow_signed,   "R_EPIPHANY_IMM8",    0x0000ff00, 0x0000ff00),

  /* %CACHEHIGH(EA) */
  AHOW (R_EPIPHANY_CACHEHIGH, 0, 2,16, FALSE, 0, complain_overflow_dont,     "R_EPIPHANY_CACHEHIGH", 0x0ff01fe0, 0x0ff01fe0),
  /* %CACHELOW(EA) */
  AHOW (R_EPIPHANY_CACHELOW,  0, 2,16, FALSE, 0, complain_overflow_dont,     "R_EPIPHANY_CACHELOW",  0x0ff01fe0, 0x0ff01fe0),

  /* A 32 bit PLT relocation for the software cache. */
  AHOW (R_EPIPHANY_CACHE32,      0, 2,32, FALSE, 0, complain_overflow_dont,     "R_EPIPHANY_CACHE32",     0xffffffff, 0xffffffff)

};
#undef AHOW

/* Map BFD reloc types to EPIPHANY ELF reloc types.  */

static reloc_howto_type *
epiphany_reloc_type_lookup (bfd * abfd ATTRIBUTE_UNUSED,
			    bfd_reloc_code_real_type code)
{
  /* Note that the epiphany_elf_howto_table is indxed by the R_
     constants.  Thus, the order that the howto records appear in the
     table *must* match the order of the relocation types defined in
     include/elf/epiphany.h.  */

  switch (code)
    {
    case BFD_RELOC_NONE:
      return &epiphany_elf_howto_table[ (int) R_EPIPHANY_NONE];

    case BFD_RELOC_EPIPHANY_SIMM8:
      return &epiphany_elf_howto_table[ (int) R_EPIPHANY_SIMM8];
    case BFD_RELOC_EPIPHANY_SIMM24:
      return &epiphany_elf_howto_table[ (int) R_EPIPHANY_SIMM24];

    case BFD_RELOC_8_PCREL:
      return &epiphany_elf_howto_table[ (int) R_EPIPHANY_8_PCREL];
    case BFD_RELOC_16_PCREL:
      return &epiphany_elf_howto_table[ (int) R_EPIPHANY_16_PCREL];
    case BFD_RELOC_32_PCREL:
      return &epiphany_elf_howto_table[ (int) R_EPIPHANY_32_PCREL];

    case BFD_RELOC_8:
      return &epiphany_elf_howto_table[ (int) R_EPIPHANY_8];
    case BFD_RELOC_16:
      return &epiphany_elf_howto_table[ (int) R_EPIPHANY_16];
    case BFD_RELOC_32:
      return &epiphany_elf_howto_table[ (int) R_EPIPHANY_32];

    case BFD_RELOC_EPIPHANY_HIGH:
      return & epiphany_elf_howto_table[ (int) R_EPIPHANY_HIGH];
    case BFD_RELOC_EPIPHANY_LOW:
      return & epiphany_elf_howto_table[ (int) R_EPIPHANY_LOW];

    case BFD_RELOC_EPIPHANY_SIMM11:
      return & epiphany_elf_howto_table[ (int) R_EPIPHANY_SIMM11];
    case BFD_RELOC_EPIPHANY_IMM11:
      return & epiphany_elf_howto_table[ (int) R_EPIPHANY_IMM11];

    case BFD_RELOC_EPIPHANY_IMM8:
      return & epiphany_elf_howto_table[ (int) R_EPIPHANY_IMM8];

    case BFD_RELOC_EPIPHANY_CACHEHIGH:
      return & epiphany_elf_howto_table[ (int) R_EPIPHANY_CACHEHIGH];
    case BFD_RELOC_EPIPHANY_CACHELOW:
      return & epiphany_elf_howto_table[ (int) R_EPIPHANY_CACHELOW];

    case BFD_RELOC_EPIPHANY_CACHE32:
      return & epiphany_elf_howto_table[ (int) R_EPIPHANY_CACHE32];

    default:
      /* Pacify gcc -Wall.  */
      return NULL;
    }
  return NULL;
}

static reloc_howto_type *
epiphany_reloc_name_lookup (bfd *abfd ATTRIBUTE_UNUSED, const char *r_name)
{
  unsigned int i;

  for (i = 0; i < ARRAY_SIZE (epiphany_elf_howto_table); i++)
    if (epiphany_elf_howto_table[i].name != NULL
	&& strcasecmp (epiphany_elf_howto_table[i].name, r_name) == 0)
      return &epiphany_elf_howto_table[i];

  return NULL;
}

#define PAGENO(ABSADDR) ((ABSADDR) & 0xFFFFC000)
#define BASEADDR(SEC)	((SEC)->output_section->vma + (SEC)->output_offset)

/* This function handles relaxing for the epiphany.
   Dummy placeholder for future optimizations.  */

static bfd_boolean
epiphany_elf_relax_section (bfd *abfd, asection *sec,
			    struct bfd_link_info *link_info,
			    bfd_boolean *again)
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Rela *internal_relocs;
  bfd_byte *contents = NULL;
  Elf_Internal_Sym *isymbuf = NULL;
  static asection * first_section = NULL;
  static unsigned long search_addr;
  static unsigned long page_start = 0;
  static unsigned long page_end = 0;
  static unsigned int pass = 0;
  static bfd_boolean new_pass = FALSE;
  static bfd_boolean changed = FALSE;
  struct misc misc ATTRIBUTE_UNUSED;
  asection *stab;

  /* Assume nothing changes.  */
  *again = FALSE;

  if (first_section == NULL)
    {
      epiphany_relaxed = TRUE;
      first_section = sec;
    }

  if (first_section == sec)
    {
      pass++;
      new_pass = TRUE;
    }

  /* We don't have to do anything for a relocatable link,
     if this section does not have relocs, or if this is
     not a code section.  */
  if (link_info->relocatable
      || (sec->flags & SEC_RELOC) == 0
      || sec->reloc_count == 0
      || (sec->flags & SEC_CODE) == 0)
    return TRUE;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;

  internal_relocs = _bfd_elf_link_read_relocs (abfd, sec, NULL, NULL,
					       link_info->keep_memory);
  if (internal_relocs == NULL)
    goto error_return;

  /* Make sure the stac.rela stuff gets read in.  */
  stab = bfd_get_section_by_name (abfd, ".stab");

  if (stab)
    {
      /* So stab does exits.  */
      Elf_Internal_Rela * irelbase ATTRIBUTE_UNUSED;

      irelbase = _bfd_elf_link_read_relocs (abfd, stab, NULL, NULL,
					    link_info->keep_memory);
    }

  /* Get section contents cached copy if it exists.  */
  if (contents == NULL)
    {
      /* Get cached copy if it exists.  */
      if (elf_section_data (sec)->this_hdr.contents != NULL)
	contents = elf_section_data (sec)->this_hdr.contents;
      else
	{
	  /* Go get them off disk.  */
	  if (!bfd_malloc_and_get_section (abfd, sec, &contents))
	    goto error_return;
	}
    }

  /* Read this BFD's symbols cached copy if it exists.  */
  if (isymbuf == NULL && symtab_hdr->sh_info != 0)
    {
      isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;
      if (isymbuf == NULL)
	isymbuf = bfd_elf_get_elf_syms (abfd, symtab_hdr,
					symtab_hdr->sh_info, 0,
					NULL, NULL, NULL);
      if (isymbuf == NULL)
	goto error_return;
    }

  misc.symtab_hdr = symtab_hdr;
  misc.isymbuf = isymbuf;
  misc.irelbase = internal_relocs;
  misc.contents = contents;

  /* This is where all the relaxation actually get done.  */
  if ((pass == 1) || (new_pass && !changed))
    {
      /* On the first pass we simply search for the lowest page that
	 we havn't relaxed yet. Note that the pass count is reset
	 each time a page is complete in order to move on to the next page.
	 If we can't find any more pages then we are finished.  */
      if (new_pass)
	{
	  pass = 1;
	  new_pass = FALSE;
	  changed = TRUE; /* Pre-initialize to break out of pass 1.  */
	  search_addr = 0xFFFFFFFF;
	}

      if ((BASEADDR (sec) + sec->size < search_addr)
	  && (BASEADDR (sec) + sec->size > page_end))
	{
	  if (BASEADDR (sec) <= page_end)
	    search_addr = page_end + 1;
	  else
	    search_addr = BASEADDR (sec);

	  /* Found a page => more work to do.  */
	  *again = TRUE;
	}
    }
  else
    {
      if (new_pass)
	{
	  new_pass = FALSE;
	  changed = FALSE;
	  page_start = PAGENO (search_addr);
	  page_end = page_start | 0x00003FFF;
	}

      /* Only process sections in range.  */
      if ((BASEADDR (sec) + sec->size >= page_start)
	  && (BASEADDR (sec) <= page_end))
	{
#if 0
	  if (!epiphany_elf_relax_section_page (abfd, sec, &changed, &misc,
						page_start, page_end))
#endif
	    return FALSE;
	}
      *again = TRUE;
    }

  /* Perform some house keeping after relaxing the section.  */

  if (isymbuf != NULL
      && symtab_hdr->contents != (unsigned char *) isymbuf)
    {
      if (! link_info->keep_memory)
	free (isymbuf);
      else
	symtab_hdr->contents = (unsigned char *) isymbuf;
    }

  if (contents != NULL
      && elf_section_data (sec)->this_hdr.contents != contents)
    {
      if (! link_info->keep_memory)
	free (contents);
      else
	{
	  /* Cache the section contents for elf_link_input_bfd.  */
	  elf_section_data (sec)->this_hdr.contents = contents;
	}
    }

  if (internal_relocs != NULL
      && elf_section_data (sec)->relocs != internal_relocs)
    free (internal_relocs);

  return TRUE;

 error_return:
  if (isymbuf != NULL
      && symtab_hdr->contents != (unsigned char *) isymbuf)
    free (isymbuf);
  if (contents != NULL
      && elf_section_data (sec)->this_hdr.contents != contents)
    free (contents);
  if (internal_relocs != NULL
      && elf_section_data (sec)->relocs != internal_relocs)
    free (internal_relocs);
  return FALSE;
}

/* Set the howto pointer for a EPIPHANY ELF reloc.  */

static void
epiphany_info_to_howto_rela (bfd * abfd ATTRIBUTE_UNUSED,
			     arelent * cache_ptr,
			     Elf_Internal_Rela * dst)
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  if (r_type >= (unsigned int) R_EPIPHANY_max)
    {
      _bfd_error_handler (_("%B: invalid Epiphany reloc number: %d"), abfd, r_type);
      r_type = 0;
    }
  cache_ptr->howto = & epiphany_elf_howto_table [r_type];
}

/* Get the location of the PLT entry we should be referring to */
static bfd_vma
epiphany_get_plt_address (bfd_vma relocation,
			  bfd *   output_bfd,
			  struct elf_link_hash_entry *h)
{
  unsigned int i;
  bfd_vma  contents;
  asection *s;

  s = bfd_get_section_by_name (output_bfd, ".plt");
  BFD_ASSERT (s != 0);

  /* Look for either entry corresponding to function or fill in next entry
     Note here that i is a memory offset, so increases by entry size */
  for (i = 0; i < s->size; i += PLT_ENTRY_SIZE)
    {
      /* memory address of actual function is third 32-bit word in entry */
      contents = bfd_get_32 (output_bfd, s->contents + i + 8);
      if (contents == 0)
	{
	  bfd_put_32 (output_bfd, relocation,
		      s->contents + i + 8);
	  /* h holds the function size to store in PLT */
	  BFD_ASSERT (h != NULL);
	  bfd_put_32 (output_bfd, h->size,
		      s->contents + i + 12);
	  return s->vma + i;
       }
      else if (contents == relocation)
	{
	  return s->vma + i;
	}
    }

  return relocation;
}

/* Perform a single relocation.
   By default we use the standard BFD routines.  */

static bfd_reloc_status_type
epiphany_final_link_relocate (bfd *                        output_bfd,
			      reloc_howto_type *           howto,
			      bfd *                        input_bfd,
			      asection *                   input_section,
			      bfd_byte *                   contents,
			      Elf_Internal_Rela *          rel,
			      bfd_vma                      relocation,
			      struct elf_link_hash_entry * h)
{
  switch (howto->type)
    {
      /* Handle 16 bit immediates.  */
    case R_EPIPHANY_CACHEHIGH:
      /* In this case check PLT entry and modify relocation */
      relocation = epiphany_get_plt_address(relocation, output_bfd, h);
    case R_EPIPHANY_HIGH:
      relocation += rel->r_addend;
      relocation >>= 16;
      goto common;

    case R_EPIPHANY_CACHELOW:
      relocation = epiphany_get_plt_address(relocation, output_bfd, h);
    case R_EPIPHANY_LOW:
      relocation += rel->r_addend;
    common:
      relocation = ((relocation & 0xff00L) << 12)
	| ((relocation & 0x00ffL) << 5);
      /* Sanity check the address.  */
      if (rel->r_offset > bfd_get_section_limit (input_bfd, input_section))
	return bfd_reloc_outofrange;

      return _bfd_relocate_contents (howto, input_bfd, relocation,
				     contents + rel->r_offset);

    case R_EPIPHANY_SIMM11:
      relocation += rel->r_addend;
      /* Check signed overflow.  */
      if ((int)relocation > 1023 || (int)relocation < -1024)
	return bfd_reloc_outofrange;
      goto disp11;

    case R_EPIPHANY_IMM11:
      relocation += rel->r_addend;
      if ((unsigned int) relocation > 0x7ff)
	return bfd_reloc_outofrange;
    disp11:
      relocation = ((relocation & 7) << 5)
	|| ((relocation & 0x7f8 )  << 13);
      return _bfd_relocate_contents (howto, input_bfd, relocation,
				     contents + rel->r_offset);
    case R_EPIPHANY_CACHE32:
      /* Here we are handling a 32 bit function pointer in the .data section.
	 Check PLT entry and modify relocation.  */
      relocation = epiphany_get_plt_address (relocation, output_bfd, h);
      break;

      /* Pass others through.  */
    default:
      break;
    }

  /* Only install relocation if above tests did not disqualify it.  */
  return _bfd_final_link_relocate (howto, input_bfd, input_section,
				   contents, rel->r_offset,
				   relocation, rel->r_addend);
}

/* Relocate an EPIPHANY ELF section.

   The RELOCATE_SECTION function is called by the new ELF backend linker
   to handle the relocations for a section.

   The relocs are always passed as Rela structures; if the section
   actually uses Rel structures, the r_addend field will always be
   zero.

   This function is responsible for adjusting the section contents as
   necessary, and (if using Rela relocs and generating a relocatable
   output file) adjusting the reloc addend as necessary.

   This function does not have to worry about setting the reloc
   address or the reloc symbol index.

   LOCAL_SYMS is a pointer to the swapped in local symbols.

   LOCAL_SECTIONS is an array giving the section in the input file
   corresponding to the st_shndx field of each local symbol.

   The global hash table entry for the global symbols can be found
   via elf_sym_hashes (input_bfd).

   When generating relocatable output, this function must handle
   STB_LOCAL/STT_SECTION symbols specially.  The output symbol is
   going to be the section symbol corresponding to the output
   section, which means that the addend must be adjusted
   accordingly.  */

static bfd_boolean
epiphany_elf_relocate_section (bfd *output_bfd,
			       struct bfd_link_info *info,
			       bfd *input_bfd,
			       asection *input_section,
			       bfd_byte *contents,
			       Elf_Internal_Rela *relocs,
			       Elf_Internal_Sym *local_syms,
			       asection **local_sections)
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;

  symtab_hdr = & elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);
  relend     = relocs + input_section->reloc_count;

  for (rel = relocs; rel < relend; rel ++)
    {
      reloc_howto_type *           howto;
      unsigned long                r_symndx;
      Elf_Internal_Sym *           sym;
      asection *                   sec;
      struct elf_link_hash_entry * h;
      bfd_vma                      relocation;
      bfd_reloc_status_type        r;
      const char *                 name = NULL;
      int                          r_type ATTRIBUTE_UNUSED;

      r_type = ELF32_R_TYPE (rel->r_info);
      r_symndx = ELF32_R_SYM (rel->r_info);
      howto  = epiphany_elf_howto_table + ELF32_R_TYPE (rel->r_info);
      h      = NULL;
      sym    = NULL;
      sec    = NULL;

      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections [r_symndx];
	  relocation = BASEADDR (sec) + sym->st_value;

	  name = bfd_elf_string_from_elf_section
	    (input_bfd, symtab_hdr->sh_link, sym->st_name);
	  name = (name == NULL) ? bfd_section_name (input_bfd, sec) : name;
	}
      else
	{
	  bfd_boolean warned ATTRIBUTE_UNUSED;
	  bfd_boolean unresolved_reloc ATTRIBUTE_UNUSED;
	  bfd_boolean ignored ATTRIBUTE_UNUSED;

	  RELOC_FOR_GLOBAL_SYMBOL (info, input_bfd, input_section, rel,
				   r_symndx, symtab_hdr, sym_hashes,
				   h, sec, relocation,
				   unresolved_reloc, warned, ignored);

	  name = h->root.root.string;
	}

      if (sec != NULL && discarded_section (sec))
	RELOC_AGAINST_DISCARDED_SECTION (info, input_bfd, input_section,
					 rel, 1, relend, howto, 0, contents);

      if (info->relocatable)
	continue;

      /* Finally, the sole EPIPHANY-specific part.  */
      r = epiphany_final_link_relocate (output_bfd, howto, input_bfd,
					input_section, contents, rel,
					relocation, h);

      if (r != bfd_reloc_ok)
	{
	  const char * msg = NULL;

	  switch (r)
	    {
	    case bfd_reloc_overflow:
	      r = info->callbacks->reloc_overflow
		(info, (h ? &h->root : NULL), name, howto->name,
		 (bfd_vma) 0, input_bfd, input_section, rel->r_offset);
	      break;

	    case bfd_reloc_undefined:
	      r = info->callbacks->undefined_symbol
		(info, name, input_bfd, input_section, rel->r_offset, TRUE);
	      break;

	    case bfd_reloc_outofrange:
	      msg = _("internal error: out of range error");
	      break;

	      /* This is how epiphany_final_link_relocate tells us of a
		 non-kosher reference between insn & data address spaces.  */
	    case bfd_reloc_notsupported:
	      if (sym != NULL) /* Only if it's not an unresolved symbol.  */
		 msg = _("unsupported relocation between data/insn address spaces");
	      break;

	    case bfd_reloc_dangerous:
	      msg = _("internal error: dangerous relocation");
	      break;

	    default:
	      msg = _("internal error: unknown error");
	      break;
	    }

	  if (msg)
	    r = info->callbacks->warning
	      (info, msg, name, input_bfd, input_section, rel->r_offset);

	  if (! r)
	    return FALSE;
	}
    }

  return TRUE;
}



/* Based on _bfd_elf_create_dynamic_sections */
static bfd_boolean
epiphany_elf_create_plt_section (bfd *dynobj, struct bfd_link_info *info)
{
   flagword flags;
   struct elf_link_hash_entry *h;
   asection *s;
   const struct elf_backend_data *bed = get_elf_backend_data (dynobj);
   struct elf_link_hash_table *htab = elf_hash_table (info);
   bfd *output_bfd = info->output_bfd;
   
   /* If .plt already exists, we don't need to recreate it */
   if (htab->splt != NULL)
     return TRUE;
   
   flags  = bed->dynamic_sec_flags;
   flags |= SEC_ALLOC | SEC_CODE | SEC_LOAD;
   s = bfd_make_section_anyway_with_flags (dynobj, ".plt", flags);
   if (s == NULL
       || ! bfd_set_section_alignment (dynobj, s, bed->plt_alignment))
     return FALSE;
   htab->splt = s;

   htab->dynobj = output_bfd;
   
   /* Define PLT symbol */
   h = _bfd_elf_define_linkage_sym (dynobj, info, s,
				    "_PROCEDURE_LINKAGE_TABLE");
   htab->hplt = h;
   if (h == NULL)
     return FALSE;
  
   return TRUE;
 }

/* Check through relocations in a section, and assign space in the PLT
   where required. */
 static bfd_boolean
 epiphany_elf_check_relocs (bfd *abfd,
			    struct bfd_link_info *info ATTRIBUTE_UNUSED,
			    asection *sec ATTRIBUTE_UNUSED,
			    const Elf_Internal_Rela *relocs)
 {
   Elf_Internal_Shdr *symtab_hdr;
   struct elf_link_hash_entry **sym_hashes;
   const Elf_Internal_Rela *rel;

   const Elf_Internal_Rela *rel_end;

   symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
   sym_hashes = elf_sym_hashes (abfd);

   rel_end = relocs + sec->reloc_count;
   for (rel = relocs; rel < rel_end; rel++)
     {
       struct elf_link_hash_entry *h;
       unsigned long r_symndx;

       r_symndx = ELF32_R_SYM (rel->r_info);
       if (r_symndx < symtab_hdr->sh_info)
	 h = NULL;
       else
	 {
	   h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	   while (h->root.type == bfd_link_hash_indirect
		  || h->root.type == bfd_link_hash_warning)
	     h = (struct elf_link_hash_entry *) h->root.u.i.link;
	 }

       switch (ELF32_R_TYPE (rel->r_info))
	 {
	   /* These relocs require a plt entry */
	   case R_EPIPHANY_CACHE32:
	   case R_EPIPHANY_CACHEHIGH:
	     if (h != NULL)
	       {
		 /* Should this call be elsewhere? */
		 epiphany_elf_create_plt_section (abfd, info);
		 h->needs_plt = 1;
		 h->plt.refcount +=1;
		 /* Enables analysis of dynamic sections */
		 info->dynamic = 1;
		 /* If we have not seen this symbol before space needs
		    allocating in the PLT */
		 if (h->plt.refcount == 0)
		   elf_hash_table (info)->splt->size += PLT_ENTRY_SIZE;
	       }
	     break;
	   /* Do we need anything else here? */
	   default:
	     break;
	 }
     }

   return TRUE;
 }

/* Allocate memory for PLT contents, replacing the PLT section
   we previously created with that in the final object. */
static bfd_boolean
epiphany_elf_size_dynamic_sections (bfd *output_bfd,
				    struct bfd_link_info *info)
{
  asection *tmpplt = elf_hash_table(info)->splt;
  asection *plt = bfd_get_section_by_name (output_bfd, ".plt");
  plt->contents = bfd_zalloc (output_bfd, tmpplt->size);
  plt->size = tmpplt->size;
  elf_hash_table(info)->splt = plt;

  return TRUE;
}

/* Generate the branches to our cache manager */
static bfd_boolean
epiphany_elf_finish_dynamic_sections (bfd * output_bfd,
				      struct bfd_link_info * info ATTRIBUTE_UNUSED)
{
  asection *plt;
  asection *cacheman;
  bfd_vma   i;

  plt = bfd_get_section_by_name (output_bfd, ".plt");
  if (plt == NULL)
    return TRUE;

  /* We assume our chache manager is in .cacheman */
  cacheman = bfd_get_section_by_name (output_bfd, ".cacheman");
  BFD_ASSERT (cacheman != NULL);
  if (cacheman == NULL)
    return FALSE;

  for (i = 0; i < plt->size; i += PLT_ENTRY_SIZE)
    {
      /* str lr,[sp],-0x2*/
      bfd_put_32 (output_bfd, 0x2700d55c, plt->contents + i);
      
      bfd_vma branch = 0x000000f8; /* 32-bit relative branch with link */
      bfd_vma diff = cacheman->vma - plt->vma - i - 4;
      /* Check that the branch target is within 24-bit range
         Note signed immediates are shifted one to the left  */
      BFD_ASSERT ((signed long) diff <  33554431 &&
		  (signed long) diff > -33554432);
      diff = diff & 0x01fffffe;
      branch |= (diff << 7);
      bfd_put_32 (output_bfd, branch, plt->contents + i + 4);
    }

  return TRUE;
}


/* We only have a little-endian target.  */
#define TARGET_LITTLE_SYM	 epiphany_elf32_vec
#define TARGET_LITTLE_NAME  "elf32-epiphany"

#define ELF_ARCH	 bfd_arch_epiphany
#define ELF_MACHINE_CODE EM_ADAPTEVA_EPIPHANY

#define ELF_MAXPAGESIZE  0x8000 /* No pages on the EPIPHANY.  */

#define elf_info_to_howto_rel			NULL
#define elf_info_to_howto			epiphany_info_to_howto_rela

#define elf_backend_can_gc_sections     	1
#define elf_backend_rela_normal			1
#define elf_backend_relocate_section		epiphany_elf_relocate_section
#define elf_backend_check_relocs                epiphany_elf_check_relocs
#define elf_backend_plt_alignment               4

#define elf_symbol_leading_char			'_'
#define bfd_elf32_bfd_reloc_type_lookup		epiphany_reloc_type_lookup
#define bfd_elf32_bfd_reloc_name_lookup		epiphany_reloc_name_lookup
#define bfd_elf32_bfd_relax_section		epiphany_elf_relax_section

#define elf_backend_size_dynamic_sections       epiphany_elf_size_dynamic_sections
#define elf_backend_finish_dynamic_sections     epiphany_elf_finish_dynamic_sections

#include "elf32-target.h"
