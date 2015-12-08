
jQuery.fn.dataTable.ext.type.order['file-size-pre'] = function ( data ) {
    var matches = data.match( /^(\d+(?:\.\d+)?)\s*([a-z]+)/i );
    var multipliers = {
        b:  1,
        kb: 1000,
        mb: 1000000,
        gb: 1000000000,
        tb: 1000000000000,
        pb: 1000000000000000
    };

    var multiplier = multipliers[matches[2].toLowerCase()];
    return parseFloat( matches[1] ) * multiplier;
};
