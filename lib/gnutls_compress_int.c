/*
 * Copyright (C) 2000,2002,2003 Nikos Mavroyanopoulos
 *
 * This file is part of GNUTLS.
 *
 *  The GNUTLS library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public   
 *  License as published by the Free Software Foundation; either 
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 */

#include <gnutls_int.h>
#include <gnutls_compress.h>
#include <gnutls_algorithms.h>
#include "gnutls_errors.h"
#ifdef USE_MINILZO
# include "../libextra/minilzo.h" /* get the prototypes only.
      *	Since LZO is a GPLed library, the gnutls_global_init_extra() has
      *	to be called, before LZO compression can be used.
      */
#else
# include <lzo1x.h>
#endif

typedef int (*LZO_FUNC)();

LZO_FUNC _gnutls_lzo1x_decompress_safe = NULL;
LZO_FUNC _gnutls_lzo1x_1_compress = NULL;

/* The flag d is the direction (compressed, decompress). Non zero is
 * decompress.
 */
GNUTLS_COMP_HANDLE _gnutls_comp_init( gnutls_compression_method method, int d)
{
GNUTLS_COMP_HANDLE ret;
int err;

	ret = gnutls_malloc( sizeof( struct GNUTLS_COMP_HANDLE_STRUCT));
	if (ret==NULL) {
		gnutls_assert();
		return NULL;
	}

	ret->algo = method;
	ret->handle = NULL;

#ifdef HAVE_LIBZ
	switch( method) {
	    case GNUTLS_COMP_ZLIB: {
		int window_bits, mem_level;
		int comp_level;
		z_stream* zhandle;

		window_bits = _gnutls_compression_get_wbits( method);
		mem_level = _gnutls_compression_get_mem_level( method);
		comp_level = _gnutls_compression_get_comp_level( method);

		ret->handle = gnutls_malloc( sizeof( z_stream));
		if (ret->handle==NULL) {
			gnutls_assert();
			return NULL;
		}
		
		zhandle = ret->handle;
		
		zhandle->zalloc = (alloc_func)0;
		zhandle->zfree = (free_func)0;
		zhandle->opaque = (voidpf)0;

		if (d)
			err = inflateInit2(zhandle, window_bits);
		else {
			err = deflateInit2(zhandle, 
				comp_level, Z_DEFLATED,
				window_bits, mem_level, Z_DEFAULT_STRATEGY);
		}
		if (err!=Z_OK) {
			gnutls_assert();
			gnutls_free( ret);
			gnutls_free( ret->handle);
			return NULL;
		}
		break;
	    }
	    case GNUTLS_COMP_LZO:
	        if (d) /* LZO does not use memory on decompressor */
	           ret->handle = NULL;
	        else {
  		   ret->handle = gnutls_malloc( LZO1X_1_MEM_COMPRESS);
  		  
 		   if (ret->handle==NULL) {
			gnutls_assert();
			return NULL;
		   }
		}
		
		break;

	    default:
	    	break;
	}
#endif
	return ret;
}

void _gnutls_comp_deinit(GNUTLS_COMP_HANDLE handle, int d) {
int err;

	if (handle!=NULL) {
		switch( handle->algo) {
			case GNUTLS_COMP_LZO:
				break;
#ifdef HAVE_LIBZ
			case GNUTLS_COMP_ZLIB:
				if (d)
					err = inflateEnd( handle->handle);
				else
					err = deflateEnd( handle->handle);
				break;
#endif
			default:
				break;
		}
		gnutls_free( handle->handle);
		gnutls_free( handle);

	}

	return;
}

/* These functions are memory consuming 
 */

int _gnutls_compress( GNUTLS_COMP_HANDLE handle, const char* plain, size_t plain_size, 
	char** compressed, size_t max_comp_size) 
{
int compressed_size=GNUTLS_E_COMPRESSION_FAILED;
int err;

	/* NULL compression is not handled here
	 */
	
	switch( handle->algo) {
		case GNUTLS_COMP_LZO: {
			lzo_uint out_len;
			size_t size;
			
			if ( _gnutls_lzo1x_1_compress == NULL)
				return GNUTLS_E_COMPRESSION_FAILED;
			
			size = plain_size + plain_size / 64 + 16 + 3;
			*compressed=NULL;

			*compressed = gnutls_malloc(size);
			if (*compressed==NULL) {
				gnutls_assert();
				return GNUTLS_E_MEMORY_ERROR;
			}

		 	err = _gnutls_lzo1x_1_compress( plain, plain_size, *compressed,
		 	        &out_len, handle->handle);

		 	if (err!=LZO_E_OK) {
		 		gnutls_assert();
		 		gnutls_free( *compressed);
		 		return GNUTLS_E_COMPRESSION_FAILED;
		 	}

			compressed_size = out_len;
			break;
		}		
#ifdef HAVE_LIBZ
		case GNUTLS_COMP_ZLIB: {
			uLongf size;
			z_stream *zhandle;
			
			size = (plain_size+plain_size)+10;
			*compressed=NULL;

			*compressed = gnutls_malloc(size);
			if (*compressed==NULL) {
				gnutls_assert();
				return GNUTLS_E_MEMORY_ERROR;
			}

			zhandle = handle->handle;

			zhandle->next_in = (Bytef*) plain;
			zhandle->avail_in = plain_size;
			zhandle->next_out = (Bytef*) *compressed;
			zhandle->avail_out = size;
		
		 	err = deflate( zhandle, Z_SYNC_FLUSH);

		 	if (err!=Z_OK || zhandle->avail_in != 0) {
		 		gnutls_assert();
		 		gnutls_free( *compressed);
		 		return GNUTLS_E_COMPRESSION_FAILED;
		 	}

			compressed_size = size - zhandle->avail_out;
			break;
		}
#endif
		default:
			gnutls_assert();
			return GNUTLS_E_INTERNAL_ERROR;
	} /* switch */

#ifdef COMPRESSION_DEBUG
	_gnutls_debug_log("Compression ratio: %f\n", (float)((float)compressed_size / (float)plain_size));
#endif

	if ((size_t)compressed_size > max_comp_size) {
		gnutls_free(*compressed);
		return GNUTLS_E_COMPRESSION_FAILED;
	}

	return compressed_size;
}



int _gnutls_decompress( GNUTLS_COMP_HANDLE handle, char* compressed, size_t compressed_size, 
	char** plain, size_t max_record_size) 
{
int plain_size=GNUTLS_E_DECOMPRESSION_FAILED, err;
int cur_pos;

	if (compressed_size > max_record_size+EXTRA_COMP_SIZE) {
		gnutls_assert();
		return GNUTLS_E_DECOMPRESSION_FAILED;
	}

	/* NULL compression is not handled here
	 */
	
	switch(handle->algo) {
		case GNUTLS_COMP_LZO: {
			lzo_uint out_size;
			lzo_uint new_size;
			
			if (_gnutls_lzo1x_decompress_safe == NULL)
				return GNUTLS_E_DECOMPRESSION_FAILED;

			*plain = NULL;
			out_size = compressed_size + compressed_size;
			plain_size = 0;

			do {
				out_size += 512;
				*plain = gnutls_realloc_fast( *plain, out_size);
				if (*plain==NULL) {
					gnutls_assert();
					return GNUTLS_E_MEMORY_ERROR;
				}

			 	new_size = out_size;
			 	err = _gnutls_lzo1x_decompress_safe(compressed,compressed_size,
			 		*plain, &new_size,NULL);

			} while( (err==LZO_E_OUTPUT_OVERRUN && out_size < max_record_size));

		 	if (err!=LZO_E_OK) {
		 		gnutls_assert();
		 		gnutls_free( *plain);
		 		return GNUTLS_E_DECOMPRESSION_FAILED;
		 	}

			plain_size = new_size;
			break;
		}

#ifdef HAVE_LIBZ
		case GNUTLS_COMP_ZLIB: {
			uLongf out_size;
			z_stream* zhandle;

			*plain = NULL;
			out_size = compressed_size + compressed_size;
			plain_size = 0;

			zhandle = handle->handle;

			zhandle->next_in = (Bytef*) compressed;
			zhandle->avail_in = compressed_size;

			cur_pos = 0;
			
			do {
				out_size += 512;
				*plain = gnutls_realloc_fast( *plain, out_size);
				if (*plain==NULL) {
					gnutls_assert();
					return GNUTLS_E_MEMORY_ERROR;
				}

				zhandle->next_out = (Bytef*) (*plain + cur_pos);
				zhandle->avail_out = out_size - cur_pos;

			 	err = inflate( zhandle, Z_SYNC_FLUSH);
			 	
			 	cur_pos = out_size - zhandle->avail_out;

			} while( (err==Z_BUF_ERROR && zhandle->avail_out==0 && out_size < max_record_size) 
				|| ( err==Z_OK && zhandle->avail_in != 0));

		 	if (err!=Z_OK) {
		 		gnutls_assert();
		 		gnutls_free( *plain);
		 		return GNUTLS_E_DECOMPRESSION_FAILED;
		 	}

			plain_size = out_size - zhandle->avail_out;
			break;
		}
#endif
		default:
			gnutls_assert();
			return GNUTLS_E_INTERNAL_ERROR;
	} /* switch */

	if ((size_t)plain_size > max_record_size) {
		gnutls_assert();
		gnutls_free( *plain);
		return GNUTLS_E_DECOMPRESSION_FAILED;
	}

	return plain_size;
}
