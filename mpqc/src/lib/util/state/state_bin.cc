//
// state_bin.cc
//
// Copyright (C) 1996 Limit Point Systems, Inc.
//
// Author: Curtis Janssen <cljanss@limitpt.com>
// Maintainer: LPS
//
// This file is part of the SC Toolkit.
//
// The SC Toolkit is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// The SC Toolkit is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with the SC Toolkit; see the file COPYING.LIB.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
// The U.S. Government is granted a limited license as per AL 91-7.
//

#ifdef __GNUC__
#pragma implementation
#endif

#include <scconfig.h>
#include <util/state/state_bin.h>

#define DEBUG 0

#define CLASSNAME StateOutBin
#define PARENTS public StateOutFile
#include <util/class/classi.h>
void *
StateOutBin::_castdown(const ClassDesc*cd)
{
  void* casts[1];
  casts[0] = StateOutFile::_castdown(cd);
  return do_castdowns(casts,cd);
}

StateOutBin::StateOutBin() :
  StateOutFile()
{
  file_position_ = 0;
}

StateOutBin::StateOutBin(ostream& s):
  StateOutFile(s)
{
  file_position_ = 0;
  // needed here since only StateOutFile::open has been called so far
  put_header();
}

StateOutBin::StateOutBin(const char *path) :
  StateOutFile(path)
{
  file_position_ = 0;
  // needed here since only StateOutFile::open has been called so far
  put_header();
}

StateOutBin::~StateOutBin()
{
  // must close here since close() is overridden in this class
  close();
}

int
StateOutBin::open(const char *f)
{
  int r = StateOutFile::open(f);
  put_header();
  return r;
}

void
StateOutBin::close()
{
  if (buf_ && use_directory()) {
      int dir_loc = tell();
      seek(dir_loc_loc_);
      put_array_int(&dir_loc,1);
      seek(dir_loc);
      put_directory();
    }

  StateOutFile::close();
}

int
StateOutBin::tell()
{
  return file_position_;
}

void
StateOutBin::seek(int loc)
{
  file_position_ = loc;
#if defined(HAVE_PUBSEEKOFF)
  buf_->pubseekoff(loc,ios::beg,ios::out);
#elif defined(HAVE_SEEKOFF)
  buf_->seekoff(loc,ios::beg,ios::out);
#endif
}

int
StateOutBin::seekable()
{
#if defined(HAVE_PUBSEEKOFF) || defined(HAVE_SEEKOFF)
  return 1;
#else
  return 0;
#endif
}

int
StateOutBin::use_directory()
{
  return seekable();
}

////////////////////////////////////////////////////////////////

#define CLASSNAME StateInBin
#define PARENTS public StateInFile
#define HAVE_KEYVAL_CTOR
#include <util/class/classi.h>
void *
StateInBin::_castdown(const ClassDesc*cd)
{
  void* casts[1];
  casts[0] =  StateInFile::_castdown(cd);
  return do_castdowns(casts,cd);
}

StateInBin::StateInBin() :
  StateInFile()
{
  file_position_ = 0;
}

StateInBin::StateInBin(istream& s) :
  StateInFile(s)
{
  file_position_ = 0;
  get_header();
  find_and_get_directory();
}

StateInBin::StateInBin(const char *path) :
  StateInFile(path)
{
  file_position_ = 0;
  get_header();
  find_and_get_directory();
}

StateInBin::StateInBin(const RefKeyVal &keyval)
{
  char *path = keyval->pcharvalue("file");
  if (!path) {
      cerr << "StateInBin(const RefKeyVal&): no path given" << endl;
    }
  open(path);
  delete[] path;
}

StateInBin::~StateInBin()
{
}

int
StateInBin::open(const char *f)
{
  file_position_ = 0;
  int r = StateInFile::open(f);
  get_header();
  find_and_get_directory();
  return r;
}

int
StateInBin::tell()
{
  return file_position_;
}

void
StateInBin::seek(int loc)
{
  file_position_ = loc;
#if defined(HAVE_PUBSEEKOFF)
  buf_->pubseekoff(loc,ios::beg,ios::in);
#elif defined(HAVE_SEEKOFF)
  buf_->seekoff(loc,ios::beg,ios::in);
#endif
}

int
StateInBin::seekable()
{
#if defined(HAVE_PUBSEEKOFF) || defined(HAVE_SEEKOFF)
  return 1;
#else
  return 0;
#endif
}

int
StateInBin::use_directory()
{
  return seekable();
}

////////////////////////////////////////////////////////////////

int StateOutBin::put_array_void(const void*p,int size)
{
  if (buf_->sputn((const char *)p,size) != size) {
      cerr << "StateOutBin::put_array_void: failed" << endl;
      abort();
    }
  file_position_ += size;
  return size;
}

int StateInBin::get_array_void(void*p,int size)
{
  if (buf_->sgetn((char*)p,size) != size) {
      cerr << "StateInBin::get_array_void: failed" << endl;
      abort();
    }
#if DEBUG
  cout << "Read " << size << " bytes: ";
  for (int i=0; i<size; i++) {
      cout << ((unsigned char*)p)[i];
    }
  cout << endl;
#endif
  file_position_ += size;
  return size;
}

/////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "CLJ"
// End:
