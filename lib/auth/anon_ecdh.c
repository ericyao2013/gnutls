/*
 * Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2009, 2010 Free
 * Software Foundation, Inc.
 *
 * Author: Nikos Mavrogiannopoulos
 *
 * This file is part of GnuTLS.
 *
 * The GnuTLS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 * USA
 *
 */

/* This file contains the Anonymous Diffie-Hellman key exchange part of
 * the anonymous authentication. The functions here are used in the
 * handshake.
 */

#include <gnutls_int.h>

#ifdef ENABLE_ANON

#include "gnutls_auth.h"
#include "gnutls_errors.h"
#include "gnutls_dh.h"
#include "auth/anon.h"
#include "gnutls_num.h"
#include "gnutls_mpi.h"
#include <gnutls_state.h>
#include <auth/ecdh_common.h>
#include <ext/ecc.h>

static int gen_anon_ecdh_server_kx (gnutls_session_t, gnutls_buffer_st*);
static int proc_anon_ecdh_client_kx (gnutls_session_t, opaque *, size_t);
static int proc_anon_ecdh_server_kx (gnutls_session_t, opaque *, size_t);

const mod_auth_st anon_ecdh_auth_struct = {
  "ANON ECDH",
  NULL,
  NULL,
  gen_anon_ecdh_server_kx,
  _gnutls_gen_ecdh_common_client_kx,      /* this can be shared */
  NULL,
  NULL,

  NULL,
  NULL,                         /* certificate */
  proc_anon_ecdh_server_kx,
  proc_anon_ecdh_client_kx,
  NULL,
  NULL
};

static int
gen_anon_ecdh_server_kx (gnutls_session_t session, gnutls_buffer_st* data)
{
  ecc_curve_t curve;
  int ret;
  gnutls_anon_server_credentials_t cred;

  cred = (gnutls_anon_server_credentials_t)
    _gnutls_get_cred (session->key, GNUTLS_CRD_ANON, NULL);
  if (cred == NULL)
    {
      gnutls_assert ();
      return GNUTLS_E_INSUFFICIENT_CREDENTIALS;
    }

  curve = _gnutls_session_ecc_curve_get(session);
  if (curve == GNUTLS_ECC_CURVE_INVALID)
    return gnutls_assert_val(GNUTLS_E_ECC_NO_SUPPORTED_CURVES);
  
  if ((ret =
       _gnutls_auth_info_set (session, GNUTLS_CRD_ANON,
                              sizeof (anon_auth_info_st), 1)) < 0)
    {
      gnutls_assert ();
      return ret;
    }

  ret = _gnutls_ecdh_common_print_server_kx (session, data, curve);
  if (ret < 0)
    {
      gnutls_assert ();
    }

  return ret;
}


static int
proc_anon_ecdh_client_kx (gnutls_session_t session, opaque * data,
                     size_t _data_size)
{
  gnutls_anon_server_credentials_t cred;
  ecc_curve_t curve;

  cred = (gnutls_anon_server_credentials_t)
    _gnutls_get_cred (session->key, GNUTLS_CRD_ANON, NULL);
  if (cred == NULL)
    {
      gnutls_assert ();
      return GNUTLS_E_INSUFFICIENT_CREDENTIALS;
    }

  curve = _gnutls_session_ecc_curve_get(session);
  if (curve == GNUTLS_ECC_CURVE_INVALID)
    return gnutls_assert_val(GNUTLS_E_ECC_NO_SUPPORTED_CURVES);

  return _gnutls_proc_ecdh_common_client_kx (session, data, _data_size, curve);
}

int
proc_anon_ecdh_server_kx (gnutls_session_t session, opaque * data,
                     size_t _data_size)
{

  int ret;

  /* set auth_info */
  if ((ret =
       _gnutls_auth_info_set (session, GNUTLS_CRD_ANON,
                              sizeof (anon_auth_info_st), 1)) < 0)
    {
      gnutls_assert ();
      return ret;
    }

  ret = _gnutls_proc_ecdh_common_server_kx (session, data, _data_size);
  if (ret < 0)
    {
      gnutls_assert ();
      return ret;
    }

  return 0;
}

#endif /* ENABLE_ANON */
