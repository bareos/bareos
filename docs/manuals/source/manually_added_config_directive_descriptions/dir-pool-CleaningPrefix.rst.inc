This directive defines a prefix string, which if it matches the beginning of a Volume name during labeling of a Volume, the Volume will be defined with the VolStatus set to Cleaning and thus Bareos will never attempt to use this tape. This is primarily for use with autochangers that accept barcodes where the convention is that barcodes beginning with CLN are treated as cleaning tapes.

The default value for this directive is consequently set to CLN, so that in most cases the cleaning tapes are automatically recognized without configuration. If you use another prefix for your cleaning tapes, you can set this directive accordingly.

