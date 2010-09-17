/*
 * Droplets, high performance cloud storage client library
 * Copyright (C) 2010 Scality http://github.com/scality/Droplets
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __DROPLETS_HTTPREQ_H__
#define __DROPLETS_HTTPREQ_H__ 1

/* PROTO droplets_httpreq.c */
/* src/droplets_httpreq.c */
dpl_status_t dpl_conn_writev_all(dpl_conn_t *conn, struct iovec *iov, int n_iov, int timeout);
#endif
