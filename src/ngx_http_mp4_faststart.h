/* 
 * File:   ngx_http_mp4_faststart.h
 * Author:  - ffmpeg developers
 *          - Hung Nguyen
 *
 * Created on January 23, 2015, 11:33 AM
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_files.h>
#ifdef WIN32
#include <io.h>
#include <windows.h>
#define DIR_SEPARATOR '\\'
#define strdup _strdup
#define open _open
#define close _close
#define write _write
#define lseek _lseeki64
#define stat _stat64
#else
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <unistd.h>
#endif



#ifdef __MINGW32__
#define fseeko(x,y,z)  fseeko64(x,y,z)
#define ftello(x)      ftello64(x)
#endif

#define BE_16(x) ((((uint8_t*)(x))[0] << 8) | ((uint8_t*)(x))[1])

#define BE_32(x) ((((uint8_t*)(x))[0] << 24) | \
                  (((uint8_t*)(x))[1] << 16) | \
                  (((uint8_t*)(x))[2] << 8) | \
                   ((uint8_t*)(x))[3])

#define BE_64(x) (((uint64_t)(((uint8_t*)(x))[0]) << 56) | \
                  ((uint64_t)(((uint8_t*)(x))[1]) << 48) | \
                  ((uint64_t)(((uint8_t*)(x))[2]) << 40) | \
                  ((uint64_t)(((uint8_t*)(x))[3]) << 32) | \
                  ((uint64_t)(((uint8_t*)(x))[4]) << 24) | \
                  ((uint64_t)(((uint8_t*)(x))[5]) << 16) | \
                  ((uint64_t)(((uint8_t*)(x))[6]) << 8) | \
                  ((uint64_t)((uint8_t*)(x))[7]))

#define BE_FOURCC( ch0, ch1, ch2, ch3 )             \
        ( (uint32_t)(unsigned char)(ch3) |          \
        ( (uint32_t)(unsigned char)(ch2) << 8 ) |   \
        ( (uint32_t)(unsigned char)(ch1) << 16 ) |  \
        ( (uint32_t)(unsigned char)(ch0) << 24 ) )

#define QT_ATOM BE_FOURCC

/* top level atoms */
#define FREE_ATOM QT_ATOM('f', 'r', 'e', 'e')

#define JUNK_ATOM QT_ATOM('j', 'u', 'n', 'k')

#define MDAT_ATOM QT_ATOM('m', 'd', 'a', 't')

#define MOOV_ATOM QT_ATOM('m', 'o', 'o', 'v')

#define PNOT_ATOM QT_ATOM('p', 'n', 'o', 't')

#define SKIP_ATOM QT_ATOM('s', 'k', 'i', 'p')

#define WIDE_ATOM QT_ATOM('w', 'i', 'd', 'e')

#define PICT_ATOM QT_ATOM('P', 'I', 'C', 'T')

#define FTYP_ATOM QT_ATOM('f', 't', 'y', 'p')

#define UUID_ATOM QT_ATOM('u', 'u', 'i', 'd')

#define CMOV_ATOM QT_ATOM('c', 'm', 'o', 'v')

#define STCO_ATOM QT_ATOM('s', 't', 'c', 'o')

#define CO64_ATOM QT_ATOM('c', 'o', '6', '4')

#define ATOM_PREAMBLE_SIZE 8

#define COPY_BUFFER_SIZE 1024


/* we take 2 arguments from ngx_http_mp4_module
 * path: to open it in write mode once source file need to be modified.
 * file descriptor: nginx already opened the file, we dont have to open again 
 * ngx_fd_t is actually an integer (see ngx_files.h) 
 */


/*TODO: write system call is not atomic, should check if write does not complete
 its takes, we should comeback and write again*/

int ngx_http_enable_fast_start(ngx_str_t *path, ngx_fd_t
ngx_open_file_cached_fd, ngx_http_request_t *r) {
    unsigned char atom_bytes[ATOM_PREAMBLE_SIZE];
    uint32_t atom_type = 0;
    uint64_t atom_size = 0;
    uint64_t atom_offset = 0;
    uint64_t last_offset;
    unsigned char *moov_atom = NULL;
    unsigned char *ftyp_atom = NULL;
    uint64_t moov_atom_size;
    uint64_t ftyp_atom_size = 0;
    uint64_t i, j;
    uint32_t offset_count;
    uint64_t current_offset;
    uint64_t start_offset = 0;
    int outfile_fd = -1;
    unsigned char *temp_buf = NULL;
    ngx_log_t *log = r->connection->log;


    /* traverse through the atoms in the file to make sure that 'moov' is
     * at the end */
    while (1) {
        
        if (read(ngx_open_file_cached_fd, atom_bytes, ATOM_PREAMBLE_SIZE) == 0)
            break;
        
        atom_size = (uint32_t) BE_32(&atom_bytes[0]);
        atom_type = BE_32(&atom_bytes[4]);
        /* keep ftyp atom */
        
        if (atom_type == FTYP_ATOM) {
            ftyp_atom_size = atom_size;
            
            if (ftyp_atom) {
                ngx_pfree(r->connection->pool, ftyp_atom);
            }
            
            ftyp_atom = ngx_palloc(r->connection->pool, ftyp_atom_size);
            //            ftyp_atom = malloc(ftyp_atom_size);
            
            if (!ftyp_atom) {
                ngx_log_error(NGX_LOG_ERR, log, ngx_errno, "could not allocate "
                        "%"PRIu64" byte for ftyp atom\n", atom_size);
                goto error_out;
            }
            
            lseek(ngx_open_file_cached_fd, -ATOM_PREAMBLE_SIZE, SEEK_CUR);
            ngx_log_debug(NGX_LOG_DEBUG, log, 0, "atom_size: "
                    "%"PRIu64" \n", atom_size);
            
            if (read(ngx_open_file_cached_fd, ftyp_atom, atom_size) < 0) {
                perror((const char *) path->data);
                goto error_out;
            }
            
            start_offset = atom_size;
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                    "start_offset to verify: %"PRIu64" \n", start_offset);
            
        } else {
            /* 64-bit special case */
            if (atom_size == 1) {
                if (read(ngx_open_file_cached_fd, atom_bytes,
                        ATOM_PREAMBLE_SIZE) == 0) {
                    break;
                }
                
                atom_size = BE_64(&atom_bytes[0]);
                lseek(ngx_open_file_cached_fd, atom_size -
                        ATOM_PREAMBLE_SIZE * 2, SEEK_CUR);
                
            } else {
                lseek(ngx_open_file_cached_fd, atom_size - ATOM_PREAMBLE_SIZE,
                        SEEK_CUR);
            }
            
        }
        
        ngx_log_debug(NGX_LOG_DEBUG_HTTP, log, 0, "%c%c%c%c %10"PRIu64""
                " %"PRIu64"\n",
                (atom_type >> 24) & 255,
                (atom_type >> 16) & 255,
                (atom_type >> 8) & 255,
                (atom_type >> 0) & 255,
                atom_offset,
                atom_size);
        
        if ((atom_type != FREE_ATOM) &&
                (atom_type != JUNK_ATOM) &&
                (atom_type != MDAT_ATOM) &&
                (atom_type != MOOV_ATOM) &&
                (atom_type != PNOT_ATOM) &&
                (atom_type != SKIP_ATOM) &&
                (atom_type != WIDE_ATOM) &&
                (atom_type != PICT_ATOM) &&
                (atom_type != UUID_ATOM) &&
                (atom_type != FTYP_ATOM)) {
            ngx_log_error(NGX_LOG_ERR, log, ngx_errno, "encountered non-QT "
                    "top-level atom (is this a Quicktime file?)\n");
            break;
        }
        
        atom_offset += atom_size;

        /* The atom header is 8 (or 16 bytes), if the atom size (which
         * includes these 8 or 16 bytes) is less than that, we won't be
         * able to continue scanning sensibly after this atom, so break. */
        if (atom_size < 8)
            break;
    }

    if (atom_type != MOOV_ATOM) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "last atom in file: "
                "%s was not a moov atom\n", path->data);
        if (ftyp_atom) ngx_pfree(r->connection->pool, ftyp_atom);
        // dont close file, not our job
        return NGX_OK;
    }

    /* moov atom was, in fact, the last atom in the chunk; load the whole
     * moov atom */
    last_offset = lseek(ngx_open_file_cached_fd, -atom_size, SEEK_END);
    moov_atom_size = atom_size;
    moov_atom = ngx_palloc(r->connection->pool, moov_atom_size);
    
    if (!moov_atom) {
        ngx_log_error(NGX_LOG_ERR, log, ngx_errno, "could not allocate "
                "%"PRIu64" byte for moov atom\n", atom_size);
        goto error_out;
    }
    
    if (read(ngx_open_file_cached_fd, moov_atom, atom_size) < 0) {
        perror((const char *) path->data);
        goto error_out;
    }

    /* this utility does not support compressed atoms yet, so disqualify
     * files with compressed QT atoms */
    if (BE_32(&moov_atom[12]) == CMOV_ATOM) {
        ngx_log_error(NGX_LOG_ERR, log, ngx_errno, "this module does "
                "not support compressed moov atoms yet\n");
        ngx_pfree(r->connection->pool, ftyp_atom);
        if (moov_atom) {
            ngx_pfree(r->connection->pool, moov_atom);
        }
        /* should not return error, if we cannot fix it, 
         * let player download the whole file then play it*/
        return NGX_OK;
    }

    /* read next move_atom_size bytes
     * since we read/write file in same time, we must read before write into
     * the buffer
     */
    temp_buf = ngx_palloc(r->connection->pool, moov_atom_size);
    
    if (!temp_buf) {
        ngx_log_error(NGX_LOG_ERR, log, ngx_errno, "Cannot allocate %"PRIu64" "
                "byte for temp buf \n", moov_atom_size);
        goto error_out;
    }
    
    /* seek to after ftyp_atom */
    lseek(ngx_open_file_cached_fd, ftyp_atom_size, SEEK_SET);
    
    if (read(ngx_open_file_cached_fd, temp_buf, moov_atom_size) < 0) {
        perror((const char *) path->data);
        goto error_out;
    }
    
    start_offset += moov_atom_size;

    /* end read temp buffer bytes */

    /* crawl through the moov chunk in search of stco or co64 atoms */
    for (i = 4; i < moov_atom_size - 4; i++) {
        atom_type = BE_32(&moov_atom[i]);
        
        if (atom_type == STCO_ATOM) {
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "%s patching stco "
                    "atom...\n", path->data);
            atom_size = BE_32(&moov_atom[i - 4]);
            
            if (i + atom_size - 4 > moov_atom_size) {
                ngx_log_error(NGX_LOG_ERR, log, ngx_errno, " bad atom size\n");
                goto error_out;
            }
            
            offset_count = BE_32(&moov_atom[i + 8]);
            
            for (j = 0; j < offset_count; j++) {
                current_offset = BE_32(&moov_atom[i + 12 + j * 4]);
                current_offset += moov_atom_size;
                moov_atom[i + 12 + j * 4 + 0] = (current_offset >> 24) & 0xFF;
                moov_atom[i + 12 + j * 4 + 1] = (current_offset >> 16) & 0xFF;
                moov_atom[i + 12 + j * 4 + 2] = (current_offset >> 8) & 0xFF;
                moov_atom[i + 12 + j * 4 + 3] = (current_offset >> 0) & 0xFF;
            }
            
            i += atom_size - 4;
            
        } else if (atom_type == CO64_ATOM) {
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "%s patching co64 "
                    "atom...\n", path->data);
            atom_size = BE_32(&moov_atom[i - 4]);
            
            if (i + atom_size - 4 > moov_atom_size) {
                ngx_log_error(NGX_LOG_ERR, log, ngx_errno, " bad atom size\n");
                goto error_out;
            }
            
            offset_count = BE_32(&moov_atom[i + 8]);
            
            for (j = 0; j < offset_count; j++) {
                current_offset = BE_64(&moov_atom[i + 12 + j * 8]);
                current_offset += moov_atom_size;
                moov_atom[i + 12 + j * 8 + 0] = (current_offset >> 56) & 0xFF;
                moov_atom[i + 12 + j * 8 + 1] = (current_offset >> 48) & 0xFF;
                moov_atom[i + 12 + j * 8 + 2] = (current_offset >> 40) & 0xFF;
                moov_atom[i + 12 + j * 8 + 3] = (current_offset >> 32) & 0xFF;
                moov_atom[i + 12 + j * 8 + 4] = (current_offset >> 24) & 0xFF;
                moov_atom[i + 12 + j * 8 + 5] = (current_offset >> 16) & 0xFF;
                moov_atom[i + 12 + j * 8 + 6] = (current_offset >> 8) & 0xFF;
                moov_atom[i + 12 + j * 8 + 7] = (current_offset >> 0) & 0xFF;
            }
            
            i += atom_size - 4;
        }
    }


    if (start_offset > 0) { /* seek after ftyp atom */
        lseek(ngx_open_file_cached_fd, start_offset, SEEK_SET);
        last_offset -= start_offset;
    }

    outfile_fd = open((const char *) path->data, O_WRONLY);
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "outfile fd: %d\n", outfile_fd);
    
    if (outfile_fd < 0) {
        perror((const char *) path->data);
        goto error_out;
    }

    /* dump the same ftyp atom */
    if (ftyp_atom_size > 0) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "%s: writing ftyp atom...\n"
                , path->data);
        
        /*ssize_t ngx_write_file(ngx_file_t *file, u_char *buf, size_t size,
    off_t offset); */
        if (write(outfile_fd, ftyp_atom, ftyp_atom_size) < 0) {
            perror((const char *) path->data);
            goto error_out;
        }
    }

    i = 0;
    /*
     we must use 2 buffer to read/write 
     */
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, " moov_atom_size: %"PRIu64" \n"
            , moov_atom_size);
    
    while (last_offset) {
        //        printf("last offset: %"PRIu64"  \n", last_offset);
        if (i == 0) {
            ngx_log_debug(NGX_LOG_DEBUG, log, 0, " writing moov atom...\n");
            i = 1;
        }
        
        if (write(outfile_fd, moov_atom, moov_atom_size) < 0) {
            perror((const char *) path->data);
            goto error_out;
        }
        
        if (last_offset < moov_atom_size)
            moov_atom_size = last_offset;
        
        
        if (read(ngx_open_file_cached_fd, moov_atom, moov_atom_size) < 0) {
            perror((const char *) path->data);
            goto error_out;
        }
        
        last_offset -= moov_atom_size;

        if (write(outfile_fd, temp_buf, moov_atom_size) < 0) {
            perror((const char *) path->data);
            goto error_out;
        }
        
        if (last_offset < moov_atom_size)
            moov_atom_size = last_offset;
        
        if (read(ngx_open_file_cached_fd, temp_buf, moov_atom_size) < 0) {
            perror((const char *) path->data);
            goto error_out;
        }
        
        last_offset -= moov_atom_size;
    }
    
    /* seek to beginning of source file*/
    lseek(ngx_open_file_cached_fd, 0, SEEK_SET);

    close(outfile_fd);
    ngx_pfree(r->connection->pool, moov_atom);
    ngx_pfree(r->connection->pool, ftyp_atom);
    ngx_pfree(r->connection->pool, temp_buf);
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, " finish fixing file: %s\n"
            , path->data);
    return NGX_OK;

error_out:
    if (outfile_fd > 0)
        close(outfile_fd);

    if (moov_atom)
        ngx_pfree(r->connection->pool, moov_atom);

    if (ftyp_atom)
        ngx_pfree(r->connection->pool, ftyp_atom);

    if (temp_buf)
        ngx_pfree(r->connection->pool, temp_buf);

    return NGX_ERROR;
}
