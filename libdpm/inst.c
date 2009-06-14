/*
 * Copyright (C) 2009 Marius Vollmer <marius.vollmer@gmail.com>
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

#define _GNU_SOURCE

#include "dyn.h"
#include "db.h"
#include "inst.h"

void
dpm_install (dpm_version ver)
{
  dpm_package pkg = dpm_ver_package (ver);
  dpm_version old = dpm_db_installed (pkg);

  if (old)
    {
      ss_val old_version = dpm_ver_version (old);
      ss_val new_version = dpm_ver_version (ver);
      int cmp = dpm_db_compare_versions (new_version, old_version);

      if (cmp > 0)
	dyn_print ("Upgrading %r %r to version %r\n",
		   dpm_pkg_name (pkg),
		   old_version,
		   new_version);
      else if (cmp < 0)
	dyn_print ("Downgrading %r %r to version %r\n",
		   dpm_pkg_name (pkg),
		   old_version,
		   new_version);
      else
	dyn_print ("Reinstalling %r %r\n",
		   dpm_pkg_name (pkg),
		   old_version,
		   new_version);
    }
  else
    dyn_print ("Installing %r %r\n", 
	       dpm_pkg_name (pkg),
	       dpm_ver_version (ver));

  dpm_db_set_installed (pkg, ver);
}

void
dpm_remove (dpm_package pkg)
{
  dpm_version old = dpm_db_installed (pkg);

  if (old)
    dyn_print ("Removing %r %r\n",
	       dpm_pkg_name (pkg),
	       dpm_ver_version (old));
  else
    dyn_print ("No need to remove %r, it is not installed\n",
	       dpm_pkg_name (pkg));

  dpm_db_set_installed (pkg, NULL);
}
