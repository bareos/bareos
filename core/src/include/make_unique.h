/*
 * BAREOS® - Backup Archiving REcovery Open Sourced
 *
 * Copyright (C) 2019-2019 Bareos GmbH & Co. KG
 *
 * This program is Free Software; you can redistribute it and/or modify
 * it under the terms of version three of the GNU Affero General Public License
 * as published by the Free Software Foundation and included in the file
 * LICENSE.
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
 * details. You should have received a copy of the GNU Affero General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 *
 */

/*
 * This headerfile allows you to use the make_unique functionality from C++14
 * even though we only use a C++11 compiler.
 * The code was taken from https://isocpp.org/files/papers/N3656.txt which is
 * the basis of what has been implemented in C++14 and should be compatible in
 * every way.
 */
#ifndef BAREOS_INCLUDE_MAKE_UNIQUE_H_
#define BAREOS_INCLUDE_MAKE_UNIQUE_H_ 1

/* make sure we only add this on a C++11 compiler */
#if __cplusplus == 201103L

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace std {
template <class T>
struct _Unique_if {
  typedef unique_ptr<T> _Single_object;
};

template <class T>
struct _Unique_if<T[]> {
  typedef unique_ptr<T[]> _Unknown_bound;
};

template <class T, size_t N>
struct _Unique_if<T[N]> {
  typedef void _Known_bound;
};

template <class T, class... Args>
typename _Unique_if<T>::_Single_object make_unique(Args&&... args) {
  return unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template <class T>
typename _Unique_if<T>::_Unknown_bound make_unique(size_t n) {
  typedef typename remove_extent<T>::type U;
  return unique_ptr<T>(new U[n]());
}

template <class T, class... Args>
typename _Unique_if<T>::_Known_bound make_unique(Args&&...) = delete;
}  // namespace std
#endif /** __cplusplus == 201103L */
#endif /** BAREOS_INCLUDE_MAKE_UNIQUE_H_ */
