/*
 * Copyright (C) 2008 Marius Vollmer <marius.vollmer@gmail.com>
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "dpm.h"

void *
dpm_xmalloc (size_t size)
{
  void *mem = malloc (size);
  if (mem == NULL)
    {
      fprintf (stderr, "Out of memory.\n");
      abort ();
    }
  return mem;
}

void *
dpm_xremalloc (void *old, size_t size)
{
  void *mem = realloc (old, size);
  if (mem == NULL)
    {
      fprintf (stderr, "Out of memory.\n");
      abort ();
    }
  return mem;
}

void *
dpm_xstrdup (const char *str)
{
  char *dup;

  if (str == NULL)
    return NULL;

  dup = dpm_xmalloc (strlen (str) + 1);
  strcpy (dup, str);
  return dup;
}

void *
dpm_xstrndup (const char *str, int n)
{
  char *dup;

  if (str == NULL)
    return NULL;

  dup = strndup (str, n);
  if (dup == NULL)
    {
      fprintf (stderr, "Out of memory.\n");
      abort ();
    }
  return dup;
}
