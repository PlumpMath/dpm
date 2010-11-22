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

#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <dlfcn.h>

#include "dpm.h"

#include "testlib.h"

// Utilities

#define S(x) dyn_from_string(x)
#define Q(x) dyn_read_string(#x)

dyn_val
testsrc (const char *name)
{
  const char *dir = getenv ("TESTDATA");
  if (dir == NULL)
    dir = "./test-data";
  return dyn_format ("%s/%s", dir, name);
}

dyn_val
testdst (const char *name)
{
  if (mkdir ("./test-data", 0777) < 0 && errno != EEXIST)
    dyn_error ("Can't create ./test-data: %m");
  return dyn_format ("./test-data/%s", name);
}

bool
streqn (const char *a, const char *b, int b_len)
{
  return strncmp (a, b, b_len) == 0 && a[b_len] == '\0';
}

// Main

int
main (int argc, char **argv)
{
  return test_main (argc, argv);
}

// Tests

DEFTEST (null)
{
}

DEFTEST (dyn_alloc)
{
  // Allocate some memory and access all of it.
  char *mem = dyn_malloc (100);
  for (int i = 0; i < 100; i++)
    mem[i] = 12;

  // Reallocate it.
  mem = dyn_realloc (mem, 200);
  for (int i = 0; i < 100; i++)
    EXPECT (mem[i] == 12);
  for (int i = 100; i < 200; i++)
    mem[i] = 12;

  // Dup it
  char *mem2 = dyn_memdup (mem, 200);
  for (int i = 0; i < 200; i++)
    EXPECT (mem2[i] == 12);
  
  // Free it
  free (mem);
  free (mem2);
}

DEFTEST (dyn_alloc_fail)
{
  EXPECT_ABORT ("Out of memory.\n")
    {
      dyn_malloc (INT_MAX);
    }

  EXPECT_ABORT ("Out of memory.\n")
    {
      void *mem = dyn_malloc (10);
      mem = dyn_realloc (mem, INT_MAX);
    }
}

DEFTEST (dyn_strdup)
{
  char *str = dyn_strdup ("foobarbaz");
  EXPECT (strcmp (str, "foobarbaz") == 0);

  char *strn = dyn_strndup ("foobarbaz", 6);
  EXPECT (strcmp (strn, "foobar") == 0);

  char *null = dyn_strdup (NULL);
  EXPECT (null == NULL);

  free (str);
  free (strn);
}

DYN_DECLARE_STRUCT_ITER (int, range, int start, int stop)
{
  int cur;
  int stop;
};

void
range_init (range *iter, int start, int stop) 
{
  iter->cur = start;
  iter->stop = stop;
}

void range_fini (range *iter) { }
void range_step (range *iter) { iter->cur += 1; }
bool range_done (range *iter) { return iter->cur >= iter->stop; }
int range_elt (range *iter) { return iter->cur; }

DEFTEST (dyn_iter)
{
  int sum = 0;
  dyn_foreach_ (x, range, 0, 10)
    sum += x;
  EXPECT (sum == 0+1+2+3+4+5+6+7+8+9);

  sum = 0;
  dyn_foreach_iter (x, range, 0, 10)
    sum += x.cur;
  EXPECT (sum == 0+1+2+3+4+5+6+7+8+9);
}

void
unwind (int for_throw, void *data)
{
  *(int *)data = 1;
}

DEFTEST (dyn_unwind)
{
  int i = 0;

  dyn_begin ();
  dyn_on_unwind (unwind, &i);
  dyn_end ();

  EXPECT (i == 1);
}

DEFTEST (dyn_block)
{
  int i = 0;

  dyn_block
    {
      dyn_on_unwind (unwind, &i);
    }

  EXPECT (i == 1);

  dyn_block
    {
      i = 0;
      break;
      dyn_on_unwind (unwind, &i);
    }

  EXPECT (i == 0);
}

DYN_DECLARE_TYPE (container);

struct container_struct {
  int i;
  double d;
};

int containers_alive = 0;

void
container_unref (dyn_type *t, void *c)
{
  containers_alive -= 1;
}

int
container_equal (void *a, void *b)
{
  container ac = a, bc = b;
  return ac->i == bc->i && ac->d == bc->d;
}

DYN_DEFINE_TYPE (container, "container");

DEFTEST (dyn_type)
{
  dyn_block
    {
      container c = dyn_new (container);
      EXPECT (dyn_is (c, container_type));
      containers_alive = 1;

      EXPECT (strcmp (dyn_type_name (c), "container") == 0);

      dyn_val cc = dyn_ref (c);
      EXPECT (cc == c);

      dyn_unref (c);
      EXPECT (containers_alive == 1);
    }
  EXPECT (containers_alive == 0);

  dyn_block
    {
      {
	dyn_begin ();
	container c = dyn_new (container);
	EXPECT (dyn_is (c, container_type));
	containers_alive = 1;
	c = dyn_end_with (c);
      }
      EXPECT (containers_alive == 1);    
    }
  EXPECT (containers_alive == 0);
}

DEFTEST (dyn_string)
{
  dyn_block
    {
      dyn_val s = dyn_from_string ("hi");
      EXPECT (dyn_is_string (s));
      EXPECT (strcmp (dyn_to_string (s), "hi") == 0);
      EXPECT (dyn_eq (s, "hi"));

      dyn_val n = dyn_from_stringn ("hi1234", 2);
      EXPECT (dyn_is_string (n));
      EXPECT (strcmp (dyn_to_string (n), "hi") == 0);
      EXPECT (dyn_eq (n, "hi"));
    }
}

DEFTEST (dyn_pair)
{
  dyn_block
    {
      dyn_val p = dyn_pair (S("1st"), S("2nd"));
      EXPECT (dyn_is_pair (p));
      EXPECT (dyn_eq (dyn_first (p), "1st"));
      EXPECT (dyn_eq (dyn_second (p), "2nd"));

      dyn_val p2 = dyn_pair (S("1st"), S("2nd"));
      EXPECT (dyn_is_pair (p2));
      EXPECT (dyn_equal (p, p2));
    }
}

DEFTEST (dyn_seq)
{
  dyn_block
    {
      dyn_seq_builder b1;
      dyn_seq_start (b1);
      dyn_seq_append (b1, S("pre"));
      dyn_val pre = dyn_seq_finish (b1);

      dyn_seq_builder b3;
      dyn_seq_start (b3);
      dyn_seq_append (b3, S("post"));
      dyn_val post = dyn_seq_finish (b3);

      dyn_seq_builder b2;
      dyn_seq_start (b2);
      dyn_seq_append (b2, S("two"));
      dyn_seq_append (b2, S("three"));
      dyn_seq_prepend (b2, S("one"));
      dyn_seq_concat_front (b2, pre);
      dyn_seq_concat_back (b2, post);
      dyn_val s = dyn_seq_finish (b2);

      EXPECT (dyn_is_seq (s));
      EXPECT (dyn_len (s) == 5);
      EXPECT (dyn_eq (dyn_elt (s, 0), "pre"));
      EXPECT (dyn_eq (dyn_elt (s, 1), "one"));
      EXPECT (dyn_eq (dyn_elt (s, 2), "two"));
      EXPECT (dyn_eq (dyn_elt (s, 3), "three"));
      EXPECT (dyn_eq (dyn_elt (s, 4), "post"));

      dyn_val ss = dyn_concat (s, s, DYN_EOS);
      dyn_val sss = dyn_seq (S("pre"), S("one"), S("two"),
			     S("three"), S("post"),
			     S("pre"), S("one"), S("two"),
			     S("three"), S("post"),
			     DYN_EOS);
      EXPECT (dyn_equal (ss, sss));
    }
}

DEFTEST (dyn_assoc)
{
  dyn_block
    {
      dyn_val a = NULL;

      a = dyn_assoc (S("key"), S("value-1"), a);
      a = dyn_assoc (S("key"), S("value-2"), a);
      a = dyn_assoc (S("key-2"), S("value"), a);

      EXPECT (dyn_eq (dyn_lookup (S("key"), a), "value-2"));
      EXPECT (dyn_eq (dyn_lookup (S("key-2"), a), "value"));
      EXPECT (dyn_lookup (S("key-3"), a) == NULL);
    }
}

void
func ()
{
}

void
func_free (void *data)
{
  *(int *)data = 1;
}

DEFTEST (dyn_func)
{
  int flag = 0;
  dyn_block
    {
      dyn_val f = dyn_func (func, &flag, func_free);
      EXPECT (dyn_is_func (f));
      EXPECT (dyn_func_code (f) == func);
      EXPECT (dyn_func_env (f) == &flag);
    }
  EXPECT (flag == 1);
}

DEFTEST (dyn_equal)
{
  dyn_block
    {
      dyn_val a = S("foo");
      dyn_val b = dyn_seq (S("foo"), S("bar"), S("baz"), DYN_EOS);
      dyn_val c = dyn_seq (S("foo"), S("bar"), S("baz"), DYN_EOS);
      dyn_val d = dyn_seq (S("foo"), S("bar"), S("bazbaz"), DYN_EOS);
      dyn_val e = dyn_seq (S("foo"), S("bar"), DYN_EOS);

      EXPECT (!dyn_equal (a, NULL));
      EXPECT (dyn_equal (a, a));
      EXPECT (!dyn_equal (a, b));
      EXPECT (dyn_equal (b, c));
      EXPECT (!dyn_equal (b, d));
      EXPECT (!dyn_equal (b, e));
    }
}

DYN_DEFINE_SCHEMA (maybe_string, (or string null));

DEFTEST (dyn_schema)
{
  dyn_block
    {
      dyn_val a = dyn_apply_schema (S("foo"),
				    Q(maybe_string));
      EXPECT (dyn_eq (a, "foo"));

      dyn_val b = dyn_apply_schema (NULL,
				    Q(maybe_string));
      EXPECT (b == NULL);

      dyn_val c = dyn_apply_schema (NULL,
				    Q((defaulted string foo)));
      EXPECT (dyn_eq (c, "foo"));

      dyn_val d = dyn_apply_schema (Q((foo bar)),
				    Q(any));
      EXPECT (dyn_equal (d, Q((foo bar))));

      dyn_val e = dyn_apply_schema (Q((foo bar)),
				    Q(seq));
      EXPECT (dyn_equal (e, Q((foo bar))));

      EXPECT_STDERR 
	(1, "value does not match schema, expecting pair: (foo bar)\n")
	{
	  dyn_apply_schema (Q((foo bar)),
			    Q(pair));
	}

      
    }
}

void
expect_numbers (dyn_input in)
{
  dyn_input_count_lines (in);

  for (int i = 0; i < 10000; i++)
    {
      char *tail;
      
      EXPECT (dyn_input_lineno (in) == i+1);
      
      dyn_input_set_mark (in);
      EXPECT (dyn_input_find (in, "\n"));
      int ii = strtol (dyn_input_mark (in), &tail, 10);
      EXPECT (tail == dyn_input_pos (in));
      EXPECT (ii == i);
      
      dyn_input_advance (in, 1);
    }
}

DEFTEST (dyn_input)
{
  dyn_block
    {
      dyn_val name = testsrc ("numbers.txt");

      EXPECT (dyn_file_exists (name));

      dyn_input in = dyn_open_file (name);
      expect_numbers (in);

      dyn_input inz = dyn_open_file (testsrc ("numbers.gz"));
      expect_numbers (inz);

      dyn_input in2 = dyn_open_file (testsrc ("numbers.bz2"));
      expect_numbers (in2);

    }
}

DEFTEST (dyn_output)
{
  dyn_block
    {
      dyn_val name = testdst ("output.txt");
      dyn_output out = dyn_create_file (name);
      for (int i = 0; i < 10000; i++)
	dyn_write (out, "%d\n", i);
      dyn_output_commit (out);

      dyn_output out2 = dyn_create_file (name);
      dyn_write (out2, "boo!\n");
      dyn_output_abort (out2);

      dyn_input in = dyn_open_file (name);

      for (int i = 0; i < 10000; i++)
	{
	  char *tail;

	  dyn_input_set_mark (in);
	  EXPECT (dyn_input_find (in, "\n"));
	  int ii = strtol (dyn_input_mark (in), &tail, 10);
	  EXPECT (tail == dyn_input_pos (in));
	  EXPECT (ii == i);
	  
	  dyn_input_advance (in, 1);
	}
    }
}

DEFTEST (dyn_read)
{
  dyn_block
    {
      dyn_val x;

      x = dyn_read_string ("foo");
      EXPECT (dyn_eq (x, "foo"));

      x = dyn_read_string ("  foo");
      EXPECT (dyn_eq (x, "foo"));

      x = dyn_read_string ("\"foo\"");
      EXPECT (dyn_eq (x, "foo"));

      x = dyn_read_string ("  \"foo\"");
      EXPECT (dyn_eq (x, "foo"));

      x = dyn_read_string ("  \"\\\\\"");
      EXPECT (dyn_eq (x, "\\"));

      x = dyn_read_string ("  \"\\n\\t\\v\"");
      EXPECT (dyn_eq (x, "\n\t\v"));

      x = dyn_read_string ("# comment\nfoo");
      EXPECT (dyn_eq (x, "foo"));

      x = dyn_read_string ("foo: bar");
      EXPECT (dyn_is_pair (x));
      EXPECT (dyn_eq (dyn_first (x), "foo"));
      EXPECT (dyn_eq (dyn_second (x), "bar"));

      x = dyn_read_string ("(foo bar)");
      EXPECT (dyn_is_seq (x));
      EXPECT (dyn_eq (dyn_elt (x, 0), "foo"));
      EXPECT (dyn_eq (dyn_elt (x, 1), "bar"));

      x = dyn_read_string ("(foo: bar)");
      EXPECT (dyn_is_seq (x));
      dyn_val y = dyn_elt (x, 0);
      EXPECT (dyn_is_pair (y));
      EXPECT (dyn_eq (dyn_first (y), "foo"));
      EXPECT (dyn_eq (dyn_second (y), "bar"));

      x = dyn_read_string ("");
      EXPECT (dyn_is_eof (x));
    }
}

dyn_var var_1[1];

DEFTEST (dyn_var)
{
  dyn_set (var_1, S("foo"));

  dyn_block
    {
      EXPECT (dyn_eq (dyn_get (var_1), "foo"));
      dyn_let (var_1, S("bar"));
      EXPECT (dyn_eq (dyn_get (var_1), "bar"));
      dyn_set (var_1, S("baz"));
      EXPECT (dyn_eq (dyn_get (var_1), "baz"));
    }
  
  EXPECT (dyn_eq (dyn_get (var_1), "foo"));
}

static void
throw_test (dyn_target *target, void *data)
{
  dyn_block
    {
      int *i = data;
      dyn_on_unwind (unwind, i);
      dyn_throw (target, S("test"));
      *i = 12;
    }
}

static void
dont_throw (dyn_target *target, void *data)
{
  int *i = data;
  *i = 12;
  return;
}

DEFTEST (dyn_catch)
{
  dyn_block
    {
      dyn_val x;
      int i;

      i = 0;
      x = dyn_catch (throw_test, &i);
      EXPECT (dyn_eq (x, "test"));
      EXPECT (i == 1);

      i = 0;
      x = dyn_catch (dont_throw, &i);
      EXPECT (x == NULL);
      EXPECT (i == 12);
    }
}

static void
unhandled_test (dyn_val val)
{
  fprintf (stderr, "unhandled test condition: %s\n", dyn_to_string (val));
  exit (1);
}

dyn_condition condition_test = {
  .name = "test",
  .unhandled = unhandled_test
};

void
signal_test (void *data)
{
  dyn_signal (&condition_test, S("foo"));
}

DEFTEST (dyn_signal)
{
  EXPECT_STDERR (1, "unhandled test condition: foo\n")
    {
      dyn_signal (&condition_test, S("foo"));
    }

  dyn_block
    {
      dyn_val x;
      x = dyn_catch_condition (&condition_test, signal_test, NULL);
      EXPECT (dyn_eq (x, "foo"));
    }
}

void
signal_error (void *data)
{
  dyn_error ("foo: %s", "bar");
}

DEFTEST (dyn_error)
{
  EXPECT_STDERR (1, "foo\n")
    {
      dyn_error ("foo");
    }

  dyn_block
    {
      dyn_val x;
      x = dyn_catch_error (signal_error, NULL);
      EXPECT (dyn_eq (x, "foo: bar"));
    }
}

DEFTEST (dyn_eval)
{
  dyn_block
    {
      dyn_val env = dyn_assoc (S("foo"), S("12"), NULL);
      dyn_val x;
      
      x = dyn_eval_string ("foo", env);
      EXPECT (dyn_eq (x, "foo"));

      x = dyn_read_string ("$foo");
      EXPECT (dyn_equal (x, dyn_read_string ("$foo")));

      x = dyn_eval_string ("$foo", env);
      EXPECT (dyn_eq (x, "12"));

      x = dyn_eval_string ("$(+ $foo 1)", env);
      EXPECT (dyn_eq (x, "13"));

    }
}

DEFTEST (store_basic)
{
  dyn_block
    {
      dyn_val name = testdst ("store.db");

      EXPECT_STDERR (1,
		     "Can't open non-existing.db: "
		     "No such file or directory\n")
	{
	  ss_open ("non-existing.db", SS_READ);
	}

      dyn_val s = ss_open (name, SS_TRUNC);
      EXPECT (ss_get_root (s) == NULL);

      dyn_val exp =
	dyn_format ("Can't lock %s: Resource temporarily unavailable\n",
		    name);

      EXPECT_STDERR (1, exp)
	{
	  ss_open (name, SS_WRITE);
	}

      ss_val x = ss_blob_new (s, 3, "foo");
      EXPECT (ss_is_blob (x));
      EXPECT (strncmp (ss_blob_start (x), "foo", 3) == 0);

      ss_set_root (s, x);
      EXPECT (ss_get_root (s) == x);

      s = ss_gc (s);
      EXPECT (ss_get_root (s) != x);
      
      x = ss_get_root (s);
      EXPECT (ss_is_blob (x));
      EXPECT (strncmp (ss_blob_start (x), "foo", 3) == 0);      
    }
}

DYN_DECLARE_STRUCT_ITER (const char *, sgb_words)
{
  dyn_input in;
  const char *cur;
};

void
sgb_words_init (sgb_words *iter)
{
  iter->in = dyn_ref (dyn_open_file (testsrc ("sgb-words.txt")));
  sgb_words_step (iter);
}

void
sgb_words_fini (sgb_words *iter)
{
  dyn_unref (iter->in); 
}

void
sgb_words_step (sgb_words *iter)
{
  dyn_input_set_mark (iter->in);
  if (dyn_input_find (iter->in, "\n"))
    {
      char *m = dyn_input_mutable_mark (iter->in);
      m[dyn_input_off (iter->in)] = '\0';
      dyn_input_advance (iter->in, 1);
      iter->cur = m;
    }
  else
    iter->cur = NULL;
}

bool
sgb_words_done (sgb_words *iter)
{
  return iter->cur == NULL;
}

const char *
sgb_words_elt (sgb_words *iter)
{ 
  return iter->cur; 
}

DYN_DECLARE_STRUCT_ITER (void, contiguous_usa)
{
  dyn_input in;
  const char *a;
  const char *b;
};

void
contiguous_usa_init (contiguous_usa *iter)
{
  iter->in = dyn_ref (dyn_open_file (testsrc ("contiguous-usa.dat")));
  contiguous_usa_step (iter);
}

void
contiguous_usa_fini (contiguous_usa *iter)
{
  dyn_unref (iter->in); 
}

void
contiguous_usa_step (contiguous_usa *iter)
{
  dyn_input_set_mark (iter->in);
  if (dyn_input_find (iter->in, " "))
    {
      char *m = dyn_input_mutable_mark (iter->in);
      m[dyn_input_off (iter->in)] = '\0';
      dyn_input_advance (iter->in, 1);
      iter->a = m;
    }
  else
    iter->a = NULL;

  dyn_input_set_mark (iter->in);
  if (dyn_input_find (iter->in, "\n"))
    {
      char *m = dyn_input_mutable_mark (iter->in);
      m[dyn_input_off (iter->in)] = '\0';
      dyn_input_advance (iter->in, 1);
      iter->b = m;
    }
  else
    iter->b = NULL;
}

bool
contiguous_usa_done (contiguous_usa *iter)
{
  return iter->a == NULL;
}

void
contiguous_usa_elt (contiguous_usa *iter)
{ 
}

DEFTEST (store_blob_vector)
{
  dyn_block
    {
      dyn_val s = ss_open (testdst ("store.db"), SS_TRUNC);
      ss_val v = ss_new (NULL, 0, 0);
      int i = 0;

      dyn_foreach_ (w, sgb_words)
	{
	  ss_val b = ss_blob_new (s, strlen (w), (void *)w);
	  EXPECT (ss_is_blob (b));
	  EXPECT (ss_len (b) == strlen (w));
	  EXPECT (strncmp (w, ss_blob_start (b), strlen (w)) == 0);

	  v = ss_insert (NULL, v, i++, b);
	}

      v = ss_copy (s, v);
      ss_set_root (s, v);
      s = ss_gc (s);
      v = ss_get_root (s);

      i = 0;
      dyn_foreach_ (w, sgb_words)
	{
	  EXPECT (ss_len (v) > i);

	  ss_val b = ss_ref (v, i++);
	  EXPECT (ss_is_blob (b));
	  EXPECT (ss_len (b) == strlen (w));
	  EXPECT (strncmp (w, ss_blob_start (b), strlen (w)) == 0);
	}
    }
}

DEFTEST (store_table)
{
  dyn_block
    {
      dyn_val s = ss_open (testdst ("store.db"), SS_TRUNC);
      ss_tab *t = ss_tab_init (s, NULL);

      ss_val blobs[6000];
      int i;

      // Put all words into the table.

      i = 0;
      dyn_foreach_ (w, sgb_words)
	{
	  ss_val b = ss_tab_intern_blob (t, strlen (w), (void *)w);

	  EXPECT (ss_is_blob (b));
	  EXPECT (ss_len (b) == strlen (w));
	  EXPECT (strncmp (w, ss_blob_start (b), strlen (w)) == 0);

	  EXPECT (i < 6000);
	  blobs[i++] = b;
	}

      // Check that they are in it.

      i = 0;
      dyn_foreach_ (w, sgb_words)
	{
	  ss_val b = ss_tab_intern_soft (t, strlen (w), (void *)w);
	  EXPECT (blobs[i] == b);
	  i += 1;
	}

      // Store the table and GC the store.  The table should
      // disappear since nothing references its entries.

      ss_set_root (s, ss_tab_finish (t));
      s = ss_gc (s);
      t = ss_tab_init (s, ss_get_root (s));

      i = 0;
      dyn_foreach_ (w, sgb_words)
	{
	  ss_val b = ss_tab_intern_soft (t, strlen (w), (void *)w);
	  EXPECT (b == NULL);
	  i += 1;
	}

      // Store the words again.

      i = 0;
      dyn_foreach_ (w, sgb_words)
	{
	  ss_val b = ss_tab_intern_blob (t, strlen (w), (void *)w);
	  blobs[i++] = b;
	}

      // Remember the first 200 words in a record and GC the rest
      // away.

      ss_val v = ss_newv (s, 0, 200, blobs);
      ss_val r = ss_new (s, 0, 2,
			 ss_tab_finish (t),
			 v);

      ss_set_root (s, r);
      s = ss_gc (s);
      r = ss_get_root (s);
      t = ss_tab_init (s, ss_ref (r, 0));
      v = ss_ref (r, 1);

      // Check that the first 200 are still there, and the rest have
      // disappeared.

      i = 0;
      dyn_foreach_ (w, sgb_words)
	{
	  ss_val b = ss_tab_intern_soft (t, strlen (w), (void *)w);
	  if (i < 200)
	    EXPECT (b == ss_ref (v, i));
	  else
	    EXPECT (b == NULL);
	  i++;
	}
    }
}

DEFTEST (store_table_foreach)
{
  dyn_block
    {
      dyn_val s = ss_open (testdst ("store.db"), SS_TRUNC);
      ss_tab *t = ss_tab_init (s, NULL);

      int count = 0;
      dyn_foreach_ (w, sgb_words)
	{
	  ss_tab_intern_blob (t, strlen (w), (void*)w);
	  count++;
	}
      
      ss_val tt = ss_tab_finish (t);
      t = ss_tab_init (s, tt);

      dyn_foreach_ (w, ss_tab_entries, t)
	{
	  EXPECT (ss_len (w) == 5);
	  count--;
	}

      EXPECT (count == 0);
    }
}

DEFTEST (store_dict_strong)
{
  dyn_block
    {
      dyn_val s = ss_open (testdst ("store.db"), SS_TRUNC);

      ss_tab *t = ss_tab_init (s, NULL);
      ss_dict *d = ss_dict_init (s, NULL, SS_DICT_STRONG);
      
      int i = 0;
      dyn_foreach_ (w, sgb_words)
	{
	  ss_val b = ss_tab_intern_blob (t, strlen(w), (void *)w);
	  ss_dict_set (d, b, ss_from_int (i));
	  i += 1;
	}

      ss_set_root (s, ss_new (s, 0, 2,
			      ss_tab_finish (t),
			      ss_dict_finish (d)));
      s = ss_gc (s);
      ss_val r = ss_get_root (s);
      t = ss_tab_init (s, ss_ref (r, 0));
      d = ss_dict_init (s, ss_ref (r, 1), SS_DICT_STRONG);

      i = 0;
      dyn_foreach_ (w, sgb_words)
	{
	  ss_val b = ss_tab_intern_soft (t, strlen(w), (void *)w);
	  EXPECT (b != NULL);

	  ss_val ii = ss_dict_get (d, b);
	  EXPECT (ss_is_int (ii));
	  EXPECT (ss_to_int (ii) == i);
	  i += 1;
	}

      ss_tab_abort (t);
      ss_dict_abort (d);
    }
}

DEFTEST (store_dict_strong_set)
{
  dyn_block
    {
      dyn_val s = ss_open (testdst ("store.db"), SS_TRUNC);

      ss_tab *t = ss_tab_init (s, NULL);
      ss_dict *d = ss_dict_init (s, NULL, SS_DICT_STRONG);
      
      dyn_foreach_iter (u, contiguous_usa)
	{
	  ss_val a = ss_tab_intern_blob (t, strlen(u.a), (void *)u.a);
	  ss_val b = ss_tab_intern_blob (t, strlen(u.b), (void *)u.b);
	  ss_dict_add (d, a, b);
	}

      ss_set_root (s, ss_new (s, 0, 2,
			      ss_tab_finish (t),
			      ss_dict_finish (d)));
      s = ss_gc (s);
      ss_val r = ss_get_root (s);
      t = ss_tab_init (s, ss_ref (r, 0));
      d = ss_dict_init (s, ss_ref (r, 1), SS_DICT_STRONG);

      dyn_foreach_iter (u, contiguous_usa)
	{
	  ss_val a = ss_tab_intern_blob (t, strlen(u.a), (void *)u.a);
	  ss_val b = ss_tab_intern_blob (t, strlen(u.b), (void *)u.b);

	  EXPECT (a != NULL);
	  EXPECT (b != NULL);

	  ss_val v = ss_dict_get (d, a);
	  bool found = false;
	  for (int i = 0; i < ss_len (v); i++)
	    {
	      if (ss_ref (v, i) == b)
		found = true;
	    }
	  EXPECT (found);
	}

      ss_tab_abort (t);
      ss_dict_abort (d);
    }
}

DEFTEST (store_dict_weak)
{
  dyn_block
    {
      dyn_val s = ss_open (testdst ("store.db"), SS_TRUNC);

      ss_tab *t = ss_tab_init (s, NULL);
      ss_dict *d = ss_dict_init (s, NULL, SS_DICT_WEAK_KEYS);
      
      int i = 0;
      dyn_foreach_ (w, sgb_words)
	{
	  ss_val b = ss_tab_intern_blob (t, strlen(w), (void *)w);
	  ss_dict_set (d, b, ss_from_int (i));
	  i += 1;
	}

      ss_set_root (s, ss_new (s, 0, 2,
			      ss_tab_finish (t),
			      ss_dict_finish (d)));
      s = ss_gc (s);
      ss_val r = ss_get_root (s);

      EXPECT (ss_ref (r, 0) == NULL);
      EXPECT (ss_ref (r, 1) == NULL);

      t = ss_tab_init (s, NULL);
      d = ss_dict_init (s, NULL, SS_DICT_WEAK_KEYS);
      ss_val v[20];

      i = 0;
      dyn_foreach_ (w, sgb_words)
	{
	  ss_val b = ss_tab_intern_blob (t, strlen(w), (void *)w);
	  ss_dict_set (d, b, ss_from_int (i));
	  if (i < 20)
	    v[i] = b;
	  i += 1;
	}
      
      ss_set_root (s, ss_new (s, 0, 3,
			      ss_tab_finish (t),
			      ss_dict_finish (d),
			      ss_newv (s, 0, 20, v)));
      s = ss_gc (s);
      r = ss_get_root (s);

      EXPECT (ss_ref (r, 0) != NULL);
      EXPECT (ss_ref (r, 1) != NULL);

      ss_val vv = ss_ref (r, 2);
      t = ss_tab_init (s, ss_ref (r, 0));
      d = ss_dict_init (s, ss_ref (r, 1), SS_DICT_WEAK_KEYS);
      
      i = 0;
      dyn_foreach_ (w, sgb_words)
	{
	  ss_val b = ss_tab_intern_soft (t, strlen(w), (void *)w);
	  if (i < 20)
	    {
	      ss_val ii = ss_dict_get (d, b);
	      EXPECT (b == ss_ref (vv, i));
	      EXPECT (ss_is_int (ii));
	      EXPECT (ss_to_int (ii) == i);
	    }
	  i += 1;
	}
    }
}

DEFTEST (store_dict_weak_set)
{
  dyn_block
    {
      dyn_val s = ss_open (testdst ("store.db"), SS_TRUNC);

      ss_tab *t = ss_tab_init (s, NULL);
      ss_dict *d = ss_dict_init (s, NULL, SS_DICT_WEAK_SETS);
      
      dyn_foreach_iter (u, contiguous_usa)
	{
	  ss_val a = ss_tab_intern_blob (t, strlen(u.a), (void *)u.a);
	  ss_val b = ss_tab_intern_blob (t, strlen(u.b), (void *)u.b);
	  ss_dict_add (d, a, b);
	}

      ss_set_root (s, ss_new (s, 0, 2,
			      ss_tab_finish (t),
			      ss_dict_finish (d)));
      s = ss_gc (s);
      ss_val r = ss_get_root (s);

      EXPECT (ss_ref (r, 0) == NULL);
      EXPECT (ss_ref (r, 1) == NULL);

      t = ss_tab_init (s, NULL);
      d = ss_dict_init (s, NULL, SS_DICT_WEAK_SETS);
      ss_val v[20];

      int i = 0;
      dyn_foreach_iter (u, contiguous_usa)
	{
	  ss_val a = ss_tab_intern_blob (t, strlen(u.a), (void *)u.a);
	  ss_val b = ss_tab_intern_blob (t, strlen(u.b), (void *)u.b);
	  ss_dict_add (d, a, b);
	  if (i < 20)
	    v[i] = b;
	  i++;
	}

      ss_set_root (s, ss_new (s, 0, 3,
			      ss_tab_finish (t),
			      ss_dict_finish (d),
			      ss_newv (s, 0, 20, v)));
      s = ss_gc (s);
      r = ss_get_root (s);
      
      EXPECT (ss_ref (r, 0) != NULL);
      EXPECT (ss_ref (r, 1) != NULL);

      ss_val vv = ss_ref (r, 2);
      t = ss_tab_init (s, ss_ref (r, 0));
      d = ss_dict_init (s, ss_ref (r, 1), SS_DICT_WEAK_SETS);

      i = 0;
      dyn_foreach_iter (u, contiguous_usa)
	{
	  ss_val a = ss_tab_intern_soft (t, strlen(u.a), (void *)u.a);
	  ss_val b = ss_tab_intern_soft (t, strlen(u.b), (void *)u.b);

	  if (i < 20)
	    {
	      EXPECT (a != NULL);
	      EXPECT (b == ss_ref (vv, i));
	      ss_val bb = ss_dict_get (d, a);
	      EXPECT (bb != NULL);
	      int found = 0;
	      for (int j = 0; j < ss_len (bb); j++)
		if (ss_ref (bb, j) == b)
		  found = 1;
	      EXPECT (found);
	    }
	  i++;
	}

    }
}

DEFTEST (store_dict_foreach)
{
  dyn_block
    {
      dyn_val s = ss_open (testdst ("store.db"), SS_TRUNC);

      ss_tab *t = ss_tab_init (s, NULL);
      ss_dict *d = ss_dict_init (s, NULL, SS_DICT_WEAK_SETS);
      
      int count = 0;
      dyn_foreach_iter (u, contiguous_usa)
	{
	  ss_val a = ss_tab_intern_blob (t, strlen(u.a), (void *)u.a);
	  ss_val b = ss_tab_intern_blob (t, strlen(u.b), (void *)u.b);
	  ss_dict_add (d, a, b);
	  count++;
	}

      int count2 = 0;
      dyn_foreach_iter (kv, ss_dict_entries, d)
	{
	  count2 += ss_len (kv.val);
	}
      EXPECT (count == count2);

      int count3 = 0;
      dyn_foreach_iter (km, ss_dict_entry_members, d)
	{
	  count3++;
	}
      EXPECT (count == count3);
    }
}

DEFTEST (parse_comma_fields)
{
  dyn_block
    {
      dyn_val fields[4];
      int i = 0;
      dyn_input in = dyn_open_string ("  foo   ,bar,,x y\tz\n z\ny  ", -1);
      dyn_foreach_iter (f, dpm_parse_comma_fields_, in)
	{
	  EXPECT (i < 4);
	  fields[i++] = dyn_from_stringn (f.field, f.len);
	}

      EXPECT (i == 4);
      EXPECT (dyn_eq (fields[0], "foo"));
      EXPECT (dyn_eq (fields[1], "bar"));
      EXPECT (dyn_eq (fields[2], ""));
      EXPECT (dyn_eq (fields[3], "x y\tz\n z\ny"));
    }
}

DEFTEST (parse_relations)
{
  dyn_block
    {
      dyn_input in = dyn_open_string ("foo | bar (>= 1.0)", -1);
      int i = 0;
      dyn_foreach_iter (r, dpm_parse_relations, in)
	{
	  switch (i)
	    {
	    case 0:
	      EXPECT (streqn ("foo", r.name, r.name_len));
	      EXPECT (r.op_len == 0);
	      EXPECT (r.version_len == 0);
	      break;
	    case 1:
	      EXPECT (streqn ("bar", r.name, r.name_len));
	      EXPECT (streqn (">=", r.op, r.op_len));
	      EXPECT (streqn ("1.0", r.version, r.version_len));
	      break;
	    }
	  i++;
	}
    }
}
