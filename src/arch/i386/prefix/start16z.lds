INCLUDE arch/i386/prefix/start16.lds

/* decompressor__size is the size of the decompressor header
 *
 * NB decompressor_uncompressed is the point to which the
 * compressed image is decompressed.
 */
decompressor__size = decompressor_uncompressed - decompressor__start ;

/* Add decompressor size to image size */
image__size += decompressor__size ;
