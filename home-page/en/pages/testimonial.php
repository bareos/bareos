<? 
require_once("inc/header.php"); 

$dat_dir = '/var/www/testimonials';
$upload_dir = '/var/www/bacula';
$password='';
// 0 No
// 1 Yes
$org_type_lst = array(
       'empty'   => "SELECT ONE",
       '100' => "Church / Religious Organization",
       '101' => "Corporation",
       '102' => "Educational Institution",
       '103' => "Government",
       '104' => "Military",
       '105' => "Non-Profit Organization",
       '107' => "Small Business",
       '106' => "Other"
);

$version_lst = array(
       'empty'   => "SELECT ONE",
       '208'   => "5.0.x",
       '207'   => "3.0.x",
       '200'   => "1.36.x",
       '201'   => "1.38.x",
       '202'   => "2.0.x",
       '203'   => "2.2.x",
       '206'   => "2.4.x",
       '204'   => "GIT master version"
);

$catalog_lst = array(
       'empty'   => "SELECT ONE",
       '300'   => "MySQL",
       '301'   => "PostgreSQL",
       '303'   => "SqLite"
);

$org_industry_lst = array(
 'empty'   => 'SELECT ONE',                     '400'  => 'Aerospace / Aeronautical',
 '401'  => 'Agriculture / Farming',             '402'  => 'Architecture / Design',
 '403'  => 'Arts',                              '404'  => 'ASP',
 '405'  => 'Banking',
 '406'  => 'Church / Religious Organization',   '407'  => 'Coaching',
 '408'  => 'Construction',                      '409'  => 'Consulting (General)',
 '410'  => 'Consulting (Information Technology)','411' => 'Defense Industry',
 '412'  => 'Education / Training',              '413'  => 'Energy Industry',
 '414'  => 'Engineering',                       '415'  => 'Entertainment (Film)',
 '416'  => 'Entertainment (Music)',             '417'  => 'Entertainment (Other)',
 '418'  => 'Event Management / Conferences',    '419'  => 'Finance / Banking / Accounting',
 '420'  => 'Food Service Industry',             '421'  => 'Government',
 '422'  => 'Healthcare / Medicine',             '423'  => 'Higher Education',
 '424'  => 'Insurance',                         '425'  => 'Internet Service Provider',
 '426'  => 'K-12 Education',                    '427'  => 'Law Enforcement / Emergency Management',
 '428'  => 'Legal',                             '429'  => 'Manufacturing (Computer Equipment)',
 '430'  => 'Manufacturing (General)',           '431'  => 'Media (Publishing, Broadcasting, etc)',
 '432'  => 'Military',                          '433'  => 'Mining',
 '434'  => 'Natural Resources / Environment',   '435'  => 'Pharmaceuticals',
 '436'  => 'Public Relations / Advertising',    '437'  => 'Real Estate',
 '438'  => 'Retail / Consumer Goods',           '439'  => 'Sales / Marketing',
 '440'  => 'Scientific Research',               '441'  => 'Sports / Recreation',
 '442'  => 'Technical College / Trade School',  '443'  => 'Telecommunications',
 '444'  => 'Transportation Industry (Air)',     '445'  => 'Transportation Industry (General)',
 '446'  => 'Transportation Industry (Marine)',  '447'  => 'Travel / Tourism / Lodging',
 '448'  => 'Travel Industry',                   '449'  => 'Utilities / Public Works',
 '450'  => 'Other'                             
);

$os_lst = array(
   'empty'  =>  'SELECT ONE',        '500' =>  'AIX',
   '501' =>  'FreeBSD',              '502' =>  'HP-UX',
   '503' =>  'Linux (Debian)',       '504' =>  'Linux (Fedora)',
   '505' =>  'Linux (Gentoo)',       '506' =>  'Linux (Mandriva)',
   '507' =>  'Linux (Other)',        '508' =>  'Linux (RedHat)',
   '509' =>  'Linux (Slackware)',    '510' =>  'Linux (Suse)',
   '511' =>  'Mac OS X',             '512' =>  'NetBSD',
   '513' =>  'OpenBSD',              '514' =>  'Other',
   '515' =>  'Solaris',              '516' =>  'Windows 2000',
   '517' =>  'Windows 2003',         '518' =>  'Windows XP',
   '519' =>  'Windows Vista'
); 

$country_lst = array(
   'empty' =>  'SELECT ONE',              '1001'  =>  'Afghanistan',
   '1002'  =>  'Albania',                 '1003'  =>  'Algeria',
   '1004'  =>  'American Samoa',          '1005'  =>  'Andorra',
   '1006'  =>  'Angola',                  '1007'  =>  'Anguilla',
   '1008'  =>  'Antarctica',              '1009'  =>  'Antigua and Barbuda',
   '1010'  =>  'Argentina',               '1011'  =>  'Armenia',
   '1012'  =>  'Aruba',                   '1013'  =>  'Australia',
   '1014'  =>  'Austria',                 '1015'  =>  'Azerbaijan',
   '1016'  =>  'Bahamas',                 '1017'  =>  'Bahrain',
   '1018'  =>  'Bangladesh',              '1019'  =>  'Barbados',
   '1020'  =>  'Belarus',                 '1021'  =>  'Belgium',
   '1022'  =>  'Belize',                  '1023'  =>  'Benin',
   '1024'  =>  'Bermuda',                 '1025'  =>  'Bhutan',
   '1026'  =>  'Bolivia',                 '1027'  =>  'Bosnia and Herzegovina',
   '1028'  =>  'Botswana',                '1029'  =>  'Bouvet Island',
   '1030'  =>  'Brazil',                  '1031'  =>  'Brunei Darussalam',
   '1032'  =>  'Bulgaria',                '1033'  =>  'Burkina Faso',
   '1034'  =>  'Burundi',                 '1035'  =>  'Cambodia',
   '1036'  =>  'Cameroon',                '1037'  =>  'Canada',
   '1038'  =>  'Cape Verde',              '1039'  =>  'Cayman Islands',
   '1040'  =>  'Central African Republic','1041'  =>  'Chad',
   '1042'  =>  'Chile',                   '1043'  =>  'China',
   '1044'  =>  'Christmas Island',        '1045'  =>  'Colombia',
   '1046'  =>  'Comoros',                 '1047'  =>  'Congo',
   '1048'  =>  'Cook Islands',            '1049'  =>  'Costa Rica',
   '1054'  =>  "Ivory Coast",             '1050'  =>  'Croatia',
   '1051'  =>  'Cuba',                    '1052'  =>  'Cyprus',
   '1053'  =>  'Czech Republic',          '1055'  =>  'Denmark',
   '1056'  =>  'Djibouti',                '1057'  =>  'Dominica',
   '1058'  =>  'Dominican Republic',      '1059'  =>  'East Timor',
   '1060'  =>  'Ecuador',                 '1061'  =>  'Egypt',
   '1062'  =>  'El Salvador',             '1063'  =>  'Equatorial Guinea',
   '1064'  =>  'Eritrea',                 '1065'  =>  'Estonia',
   '1066'  =>  'Ethiopia',                '1067'  =>  'Falkland Islands',
   '1068'  =>  'Faroe Islands',           '1069'  =>  'Fiji',
   '1070'  =>  'Finland',                 '1071'  =>  'France',
   '1072'  =>  'French Guiana',           '1073'  =>  'French Polynesia',
   '1074'  =>  'Gabon',                   '1075'  =>  'Gambia',
   '1076'  =>  'Georgia',                 '1077'  =>  'Germany',
   '1078'  =>  'Ghana',                   '1079'  =>  'Gibraltar',
   '1080'  =>  'Greece',                  '1081'  =>  'Greenland',
   '1082'  =>  'Grenada',                 '1083'  =>  'Guadeloupe',
   '1084'  =>  'Guam',                    '1085'  =>  'Guatemala',
   '1086'  =>  'Guinea',                  '1087'  =>  'Guinea-Bissau',
   '1088'  =>  'Guyana',                  '1089'  =>  'Haiti',
   '1090'  =>  'Honduras',                '1091'  =>  'Hong Kong',
   '1092'  =>  'Hungary',                 '1093'  =>  'Iceland',
   '1094'  =>  'India',                   '1095'  =>  'Indonesia',
   '1096'  =>  'Iran',                    '1097'  =>  'Iraq',
   '1098'  =>  'Ireland',                 '1099'  =>  'Israel',
   '1100'  =>  'Italy',                    '1101' =>  'Jamaica',
   '1102'  =>  'Japan',                    '1103' =>  'Jordan',
   '1104'  =>  'Kazakstan',                '1105' =>  'Kenya',
   '1106'  =>  'Kiribati',                 '1107' =>  'Kuwait',
   '1108'  =>  'Kyrgystan',                '1109' =>  'Lao',
   '1110'  =>  'Latvia',                   '1111' =>  'Lebanon',
   '1112'  =>  'Lesotho',                  '1113' =>  'Liberia',
   '1232'  =>  'Libya',                    '1114' =>  'Liechtenstein',
   '1115'  =>  'Lithuania',                '1116' =>  'Luxembourg',
   '1117'  =>  'Macau',                    '1118' =>  'Macedonia (FYR)',
   '1119'  =>  'Madagascar',               '1120' =>  'Malawi',
   '1121'  =>  'Malaysia',                 '1122' =>  'Maldives',
   '1123'  =>  'Mali',                     '1124' =>  'Malta',
   '1125'  =>  'Marshall Islands',         '1126' =>  'Martinique',
   '1127'  =>  'Mauritania',               '1128' =>  'Mauritius',
   '1129'  =>  'Mayotte',                  '1130' =>  'Mexico',
   '1131'  =>  'Micronesia',               '1132' =>  'Moldova',
   '1133'  =>  'Monaco',                   '1134' =>  'Mongolia',
   '1135'  =>  'Montserrat',               '1136' =>  'Morocco',
   '1137'  =>  'Mozambique',               '1138' =>  'Myanmar',
   '1139'  =>  'Namibia',                  '1140' =>  'Nauru',
   '1141'  =>  'Nepal',                    '1142' =>  'Netherlands',
   '1143'  =>  'Netherlands Antilles',     '1144' =>  'Neutral Zone',
   '1145'  =>  'New Caledonia',            '1146' =>  'New Zealand',
   '1147'  =>  'Nicaragua',                '1148' =>  'Niger',
   '1149'  =>  'Nigeria',                  '1150' =>  'Niue',
   '1151'  =>  'Norfolk Island',           '1152' =>  'North Korea',
   '1153'  =>  'Northern Mariana Islands', '1154' =>  'Norway',
   '1155'  =>  'Oman',                     '1156' =>  'Pakistan',
   '1157'  =>  'Palau',                    '1158' =>  'Panama',
   '1159'  =>  'Papua New Guinea',         '1160' =>  'Paraguay',
   '1161'  =>  'Peru',                     '1162' =>  'Philippines',
   '1163'  =>  'Pitcairn',                 '1164' =>  'Poland',
   '1165'  =>  'Portugal',                 '1166' =>  'Puerto Rico',
   '1167'  =>  'Qatar',                    '1168' =>  'Reunion',
   '1169'  =>  'Romania',                  '1170' =>  'Russian Federation',
   '1171'  =>  'Rwanda',                   '1172' =>  'Saint Helena',
   '1173'  =>  'Saint Kitts and Nevis',    '1174' =>  'Saint Lucia',
   '1175'  =>  'Saint Pierre and Miquelon','1231' =>  'Saint Vincent and the Grenadines',
   '1176'  =>  'Samoa',                    '1177' =>  'San Marino',
   '1178'  =>  'Sao Tome and Principe',    '1179' =>  'Saudi Arabia',
   '1180'  =>  'Senegal',                  '1227' =>  'Serbia and Montenegro',
   '1181'  =>  'Seychelles',               '1182' =>  'Sierra Leone',
   '1183'  =>  'Singapore',                '1184' =>  'Slovakia',
   '1185'  =>  'Slovenia',                 '1186' =>  'Solomon Islands',
   '1187'  =>  'Somalia',                  '1188' =>  'South Africa',
   '1189'  =>  'South Georgia',            '1190' =>  'South Korea',
   '1191'  =>  'Spain',                    '1192' =>  'Sri Lanka',
   '1193'  =>  'Sudan',                    '1194' =>  'Suriname',
   '1195'  =>  'Swaziland',                '1196' =>  'Sweden',
   '1197'  =>  'Switzerland',              '1198' =>  'Syria',
   '1199'  =>  'Taiwan',                   '1200' =>  'Tajikistan',
   '1201'  =>  'Tanzania',                 '1202' =>  'Thailand',
   '1203'  =>  'Togo',                     '1204' =>  'Tokelau',
   '1205'  =>  'Tonga',                    '1206' =>  'Trinidad and Tobago',
   '1207'  =>  'Tunisia',                  '1208' =>  'Turkey',
   '1209'  =>  'Turkmenistan',             '1210' =>  'Turks and Caicos Islands',
   '1211'  =>  'Tuvalu',                   '1212' =>  'Uganda',
   '1213'  =>  'Ukraine',                  '1214' =>  'United Arab Emirates',
   '1215'  =>  'United Kingdom',           '1216' =>  'United States of America',
   '1217'  =>  'Uruguay',                  '1218' =>  'Uzbekistan',
   '1219'  =>  'Vanuatu',                  '1233' =>  'Vatican City',
   '1220'  =>  'Venezuela',                '1221' =>  'Vietnam',
   '1222'  =>  'Virgin Islands (British)', '1223' =>  'Virgin Islands (U.S.)',
   '1224'  =>  'Wallis and Futuna Islands','1225' =>  'Western Sahara',
   '1226'  =>  'Yemen',                    '1228' =>  'Zaire',
   '1229'  =>  'Zambia',                   '1230' =>  'Zimbabwe'
);

admin_link();

if ($_REQUEST['action'] == 'Add' or $_REQUEST['action'] == 'Modify')
{

?>

<script type="text/javascript" language="JavaScript">

function validate_testimonial () {
    var alertstr = '';
    var invalid  = 0;
    var invalid_fields = new Array();
    var ok;
    var form = document.forms['form1'];
    // email: standard text, hidden, password, or textarea box
    var email = form.elements['email_address'].value;
    if (email == null || ! email.match(/^[\w\-\+\._]+\@[a-zA-Z0-9][-a-zA-Z0-9\.]*\.[a-zA-Z]+$/)) {
        alertstr += '- Invalid entry for the "Email" field\n';
        invalid++;
        invalid_fields.push('email_address');
    }

    // contact_name: standard text, hidden, password, or textarea box
    var contact_name = form.elements['contact_name'].value;
    if (contact_name == null || ! contact_name.match(/^[a-zA-Z]+[- ]?[a-zA-Z]+\s*,?([a-zA-Z]+|[a-zA-Z]+\.)?$/)) {
        alertstr += '- Invalid entry for the "Contact Name" field\n';
        invalid++;
        invalid_fields.push('contact_name');
    }

    // org_name: standard text, hidden, password, or textarea box
    var org_name = form.elements['org_name'].value;
    if (org_name == null || ! org_name.match(/^[a-zA-Z0-9();@%, :!\/]+$/)) {
        alertstr += '- Invalid entry for the "Organization Name" field\n';
        invalid++;
        invalid_fields.push('org_name');
    }
    var number = form.elements['orgtype_id'].value;
    if (number == null || ! number.match(/^[0-9]+$/)) {
        alertstr += '- Choose one of the "Organization type" options\n';
        invalid_fields.push('orgtype_id');
        invalid++;
    } 
    number = form.elements['orgindustry_id'].value;
    if (number == null || ! number.match(/^[0-9]+$/)) {
        alertstr += '- Choose one of the "Organization industry type" options\n';
        invalid_fields.push('orgindustry_id');
        invalid++;
    }
    var org_size = form.elements['org_size'].value;
    if (org_size == null || ! org_size.match(/^[0-9,\.]+$/)) {
        alertstr += '- Invalid entry for the "Organization size" field\n';
        invalid_fields.push('org_size');
        invalid++;
    }

    var bacula_version = form.elements['bacula_version'].value;
    if (bacula_version == null || ! bacula_version.match(/^[0-9]+$/)) {
        alertstr += '- Invalid entry for the "Bacula version" field\n';
        invalid_fields.push('bacula_version');
        invalid++;
    }
    var ostype = form.elements['ostype_id'].value;
    if (ostype == null || ! ostype.match(/^[0-9]+$/)) {
        alertstr += '- Choose one of the "Director OS" field\n';
        invalid_fields.push('ostype_id');
        invalid++;
    }
    var catalog = form.elements['catalog_id'].value;
    if (catalog == null || ! catalog.match(/^[0-9]+$/)) {
        alertstr += '- Choose one of the "Catalog type" field\n';
        invalid_fields.push('catalog_id');
        invalid++;
    }
    var comments = form.elements['comments'].value;
    if (comments != null && comments.match(/http:\/\//)) {
        alertstr += '- Invalid entry for the "Comments" field, we disallow spam url\n';
        invalid_fields.push('comments');
        invalid++;
    }
    comments = form.elements['hardware_comments'].value;
    if (comments != null && comments.match(/http:\/\//)) {
        alertstr += '- Invalid entry for the "Hardware comments" field, we disallow spam url\n';
        invalid_fields.push('hardware_comments');
        invalid++;
    }
    var number = form.elements['number_fd'].value;
    if (number == null || ! number.match(/^[0-9,\.]+$/)) {
        alertstr += '- Invalid entry for the "Number of Client" field\n';
        invalid_fields.push('number_fd');
        invalid++;
    } 
    number = form.elements['number_sd'].value;
    if (number == null || ! number.match(/^[0-9,\.]+$/)) {
        alertstr += '- Invalid entry for the "Number of Storage" field\n';
        invalid_fields.push('number_sd');
        invalid++;
    } 
    number = form.elements['number_dir'].value;
    if (number == null || ! number.match(/^[0-9,\.]+$/) || number > 100) {
        alertstr += '- Invalid entry for the "Number of Director" field\n';
        invalid_fields.push('number_dir');
        invalid++;
    } 
    number = form.elements['month_gb'].value;
    if (number == null || ! number.match(/^[0-9,\.]+$/)) {
        alertstr += '- Invalid entry for the "Number GB/month" field\n';
        invalid_fields.push('month_gb');
        invalid++;
    } 
    number = form.elements['number_files'].value;
    if (number == null || ! number.match(/^[0-9,\.]+$/)) {
        alertstr += '- Invalid entry for the "File number" field\n';
        invalid_fields.push('number_files');
        invalid++;
    }
    if (invalid > 0 || alertstr != '') {
        if (! invalid) invalid = 'The following';   // catch for programmer error
        alert(''+invalid+' error(s) were encountered with your submission:'+'\n\n'
                +alertstr+'\n'+'Please correct these fields and try again.');
        return false;
    }
    return true;  // all checked ok
}

</script>
<table>
<tr>
        <td class="contentTopic">
                <? echo $_REQUEST['action'] ?> Testimonial
        </td>
</tr>
<tr>
        <td class="content">

Want to let others know you're using Bacula? Submit a user profile!  Your
submission will be reviewed before being made publicly available.  We reserve
the right to edit your submission for spelling, grammar, etc. You will receive
an email when your profile has been approved for public viewing. Note that
while your contact name and email address are required (to verify
information if necessary), you can choose to have them not be published along
with your profile information.  
<br/><br/>
Fields marked with a * are required. Read the privacy notice below for
information about how this data will be used.

        </td>
</tr>

<tr>
        <td class="content">
<form name='form1' enctype="multipart/form-data" method='post' action='?page=testimonial'>
<input type='hidden' name='page' value='testimonial'>
<table border='0' class='Content'>

<td class='ItemName'>
<font color='red'>*</font>Contact Name:</td>
<td class='ItemValue'><input type='text' class='ItemValue' id='contact_name' 
name='contact_name' size='30' maxlength='100' value=''></td>
<td class='ItemName'>Publish Contact Name?</td>
<td class='ItemValue'><select name='publish_contact' class='ItemValue'>
<option id='publish_contact_0'  value='0' SELECTED>No
<option id='publish_contact_1' value='1'>Yes
</select></td>
</tr>

<tr>
<td class='ItemName'><font color='red'>*</font>Email Address:</td>
<td class='ItemValue'><input type='text' class='ItemValue' name='email_address' 
id='email_address' size='30' maxlength='150' value=''></td>
<td class='ItemName'>Publish Email Address?</td><td class='ItemValue'>
<select name='publish_email' class='ItemValue'>
<option value='0' id='publish_email_0' SELECTED>No
<option value='1' id='publish_email_1' >Yes
</select></td>
</tr>

<tr><td class='ItemName'>Job Description/Title:</td>
<td class='ItemValue'>
<input type='text' name='title' id='title' size='30' maxlength='100' value=''></td></tr>
<tr><td colspan=4><br></td></tr>

<tr><td class='ItemName'><font color='red'>*</font>Organization Name:</td>
<td class='ItemValue'>
<input type='text' class='ItemValue' name='org_name' id='org_name' 
size='30' maxlength='100' value=''>
</td><td class='ItemName'>Publish Org Name?</td><td class='ItemValue'>
<select name='publish_orgname' class='ItemValue'>
<option value='0' id='publish_orgname_0' >No
<option value='1' id='publish_orgname_1' SELECTED>Yes
</select></td>
</tr>

<tr><td class='ItemName'><font color='red'>*</font>Organization Type:</td>
<td class='ItemValue' colspan='3'>
<select name='orgtype_id' class='ItemValue'>
<?

while(list ($key, $val) = each ($org_type_lst))
{
 echo "<option id='orgtype_id_$key' value='$key'>$val\n";
}

?>
</select>
</td></tr>

<tr><td class='ItemName'><font color='red'>*</font>Organization Industry/Function:</td>
<td class='ItemValue' colspan='3'>
<select name='orgindustry_id' class='ItemValue'>
<?

while(list ($key, $val) = each ($org_industry_lst))
{
 echo "<option id='orgindustry_id_$key' value='$key'>$val\n";
}

?>
</select>
</td></tr>

<tr><td class='ItemName'>
<font color='red'>*</font>Approx. Organization Size (# of Users):</td>
<td class='ItemValue'>
<input type='text' id='org_size' class='ItemValue' name='org_size' 
size='4' maxlength='6' value=''></td>
<td class='ItemName'>Publish Org Size?</td><td class='ItemValue'><select name='publish_orgsize' class='ItemValue'>
<option id='publish_orgsize_0' value='0' >No
<option id='publish_orgsize_1' value='1' SELECTED>Yes
</select></td>
</tr>
<tr>

<td class='ItemName'>Website URL:</td>
<td class='ItemValue'>
<input type='text' class='ItemValue' id='website' name='website' size='30'
 maxlength='150' value=''></td>
<td class='ItemName'>Publish Website?</td><td class='ItemValue'>
<select name='publish_website' class='ItemValue'>
<option id='publish_website_0' value='0' >No
<option id='publish_website_1' value='1' SELECTED>Yes
</select></td>
</tr>

<tr>
<td class='ItemName'>Organization Logo :</td>
<td class='ItemValue'>
<input type="file" name="org_logo" title="png, gif or jpeg file only please"/>
</td><td><i>max width 150px. png, gif or jpeg only</i></td>
</tr>

<tr><td class='ItemName'><font color='red'>*</font>Country:</td>
<td class='ItemValue'>
<select name='country_id' class='ItemValue'>
<?
while(list ($key, $val) = each ($country_lst))
{
 echo "<option id='country_id_$key' value='$key'>$val\n";
}

?>
</select>
</td></tr>

<tr><td colspan=4><br></td></tr>
<tr><td class='ItemName'><font color='red'>*</font>Bacula version:</td>
<td class='ItemValue'>
<select name='bacula_version' class='ItemValue'>
<?

while(list ($key, $val) = each ($version_lst))
{
 echo "<option id='bacula_version_$key' value='$key'>$val\n";
}

?>
</select>
</td></tr>
<tr><td class='ItemName'><font color='red'>*</font>Director OS:</td>
<td class='ItemValue'>
<select name='ostype_id' class='ItemValue'>
<?

while(list ($key, $val) = each ($os_lst))
{
 echo "<option id='ostype_id_$key' value='$key'>$val\n";
}

?>
</select>
</td></tr>
<tr><td class='ItemName'><font color='red'>*</font>Catalog DB:</td>
<td class='ItemValue'>
<select name='catalog_id' class='ItemValue'>
<?

while(list ($key, $val) = each ($catalog_lst))
{
 echo "<option id='catalog_id_$key' value='$key'>$val\n";
}

?>
</select>
</td></tr>
<tr>
<td class='ItemName'><font color='red'>*</font>Redundant/Failover Backup Setup?</td>
<td class='ItemValue'><select name='redundant_setup' class='ItemValue'>
<option value='0' id='redundant_setup_0' SELECTED>No
<option value='1' id='redundant_setup_1' >Yes
</select></td>
</tr>

<tr><td class='ItemName'><font color='red'>*</font>Number of Director (Running bacula-dir):</td>
<td class='ItemValue'>
<input type='text' class='ItemValue' id='number_dir' name='number_dir' 
 size='5' maxlength='10' value=''></td>
</tr>

<tr><td class='ItemName'><font color='red'>*</font>Number of Clients (Running bacula-fd):</td>
<td class='ItemValue'>
<input type='text' class='ItemValue' id='number_fd' name='number_fd' 
 size='5' maxlength='10' value=''></td>
</tr>

<tr><td class='ItemName'>
<font color='red'>*</font>Number of Storage Daemons (Running bacula-sd):</td>
<td class='ItemValue'>
<input type='text' class='ItemValue' id='number_sd' name='number_sd' size='5' 
 maxlength='10' value=''></td>
</tr>

<tr><td class='ItemName' title="See bellow how to get this information"><font color='red'>*</font>Total # of GB saved every month:</td>
<td class='ItemValue'><input type='text' class='ItemValue' name='month_gb' size='5' 
 id='month_gb' maxlength='10' value=''></td>
</tr>

<tr><td class='ItemName' title='See bellow how to get this information'><font color='red'>*</font>Number # of Files:</td>
<td class='ItemValue'><input type='text' class='ItemValue' name='number_files'
 id='number_files' size='10' maxlength='15' value=''></td>
</tr>
<tr>

<td class='ItemName'>Need professional support:</td>
<td class='ItemValue'>
<select name='support' class='ItemValue'>
<option id='support_0' value='0' SELECTED>No
<option id='support_1' value='1'>Yes
</select></td>
</tr>

<tr><td colspan=4><br></td></tr>

<tr><td class='ItemName' valign='top'>Applicable Hardware and Network Information:</td>
<td class='ItemValue' colspan='3'>
<textarea name='hardware_comments' wrap='virtual' rows='4' id='hardware_comments'
 cols='60' class='ItemValue'>Loader Description:
Barcode Reader: Yes/No
Number of Storage Elements:
Number of Import/Export Elements:
--
Nics: 
...
</textarea>
</tr>

<tr><td class='ItemName' valign='top'>General Comments:</td>
<td class='ItemValue' colspan='3'>
<textarea name='comments' id='comments' wrap='virtual' rows='4' cols='60' 
 class='ItemValue'></textarea></tr>

<tr><td></td><td>
<?
 if ($_REQUEST['action'] == 'Modify') {
  echo "<input type='hidden' title='testimonial id' id='id' name='id' class='ItemValue' value=''>";
  echo "<input type='submit' name='action' class='ItemValue' onclick='return validate_testimonial();' value='Save'>";
  echo "<input type='submit' name='action' class='ItemValue' onclick='confirm(\"Are you sure ?\");' value='Delete'><br>";
  echo "<input type='hidden' name='page' class='ItemValue' value='testimonial'><br>";

} else {  
  echo "<input type='submit' name='action' class='ItemValue' onclick='return validate_testimonial(this);' value='Review Profile Submission'>";
}
?>

</td></tr>
</table>
</form>

</td>
</tr>
<tr>

        <td class="content">
        <h3 style="padding: 5px; border-bottom: 1px dotted #002244"> Getting backup information </h3>
To get <i>Total # of GB saved every month</i>, you can run this query on you catalog (just adapt the starttime condition and round the result)
<pre>
bacula@yourdir:~$ bconsole
*sql
SELECT sum(JobBytes)/1073741824 FROM Job WHERE StartTime > '2008-02-07' AND Type = 'B'
</pre>

To know how many files are in your catalog, you can run this:
<pre>
bacula@yourdir:~$ bconsole
*sql
SELECT sum(JobFiles) FROM Job WHERE Type = 'B'
</pre>
        </td>
</tr>
<tr>
        <td class="content">
        <h3 style="padding: 5px; border-bottom: 1px dotted #002244"> Privacy Notice </h3>

The following information is required, but you may choose to not have it
published for public viewing if you wish: contact name, email address,
organization name. We may use this information to verify the data you submit if
we find the need.
        </td>
</tr>


</table>
<?
}
if ($_REQUEST['action'] == 'Modify') {
   $filename = get_file_from_id();

   if (!$filename) {
        return (0);
   }

   $formul = load_formul($filename);

   echo "<script type='text/javascript' language='JavaScript'>\n";
   $attribs = array('contact_name','email_address', 'org_name','title','website',
                    'month_gb','number_files', 'number_dir','number_fd','number_sd',
                    'org_size','id');
   foreach ($attribs as $arr) {
           form_set_value($formul, $arr);
   }

   $attribs = array('publish_contact','publish_email', 'publish_orgname', 'orgtype_id', 
                    'orgindustry_id', 'publish_orgsize','publish_website', 'bacula_version',
                    'country_id','ostype_id', 'redundant_setup', 'catalog_id', 'support');
   foreach ($attribs as $arr) {
        form_set_selection($formul, $arr);
   }

   $attribs = array('comments', 'hardware_comments');
   foreach ($attribs as $arr) {
        form_set_text($formul, $arr);
   }
  

   echo "</script>\n";

} elseif ($_REQUEST['action'] == 'Review Profile Submission') {

        $form = get_formul();
        if (!$form) {
                echo "Sorry, something is missing, I can't accept your submission";
        } else {
                $token = uniqid(md5(rand()), true);
                $filename = "$dat_dir/profile.$token";
        
                $form['filename'] = $filename;  
                $form['id'] = $token;
                $form['visible']=0;
                save_formul($form);

                send_email($form['id'], $form['contact_name'], $form['email_address']);
                echo "You can modify your profile <a href='?page=testimonial&action=Modify&id=" . $form['id'] . "'>here</a> (keep this link as bookmark)<br><br>";
                print_formul($form);
        }

} elseif ($_REQUEST['action'] == 'View') {
        
        $file = get_file_from_id();

        if ($file) {
             print_formul_file($filename,true);
        }

} elseif ($_REQUEST['action'] == 'Delete') {

        $filename = get_file_from_id();

        if (!$filename) {
            return (0);
        }

        $form = load_formul($filename);
        
        if ($form['org_logo'] && file_exists($form['org_logo'])) {
            rename($upload_dir + $form['org_logo'], 'removed.' + $upload_dir + $form['org_logo']);
        }
        if (file_exists($filename)) {
            rename($filename, "$filename-removed");
            echo "Profile deleted";
        }

} elseif ($_REQUEST['action'] == 'Accept' && is_admin()) {

        $filename = get_file_from_id();
        
        if (!$filename) {
            return (0);
        }

        $form = load_formul($filename);

        $hide = $_REQUEST['hide'];

        if ($hide) {
                $form['visible']=0;
        } else {
                $form['visible']=1;
        }
        save_formul($form);
        echo $form['id'] . " is now " . ($hide?"un":"") . "visible";
        print_formul($form);

} elseif ($_REQUEST['action'] == 'Save') {
        
        $filename = get_file_from_id();

        if (!$filename) {
            return (0);
        }

        $form = get_formul();

        $form['filename'] = $filename;  
        $form['id'] = $_REQUEST['id'];  // id is clean
//      $form['visible'] = false;

        if (!$form['org_logo']) {
            $form_old = load_formul($filename);
            $form['org_logo'] = $form_old['org_logo'];
        }

        save_formul($form);

        echo "Your profile has been modified.<br>";
        print_formul($form);

} elseif ($_REQUEST['action'] == 'Admin' && is_admin()) {

    view_all();

} elseif (!$_REQUEST['action'] || $_REQUEST['action'] == 'ViewAll') {

    echo "<a href='?page=testimonial&action=Add'>Add your testimonial</a><br><br>";
    view_all();

} elseif ($_REQUEST['action'] == 'AdminExport' && is_admin()) {

   $file = get_file_from_id();
   if (!$file) {
       return 0;
   }
   $form = load_formul($file);
   if ($form) {
        export_form($form);
   }
} elseif ($_REQUEST['action'] == 'sql') {
   print "<pre>";
   dump_sql();
   print "</pre>";
}

function view_all()
{
    global $dat_dir;
    global $password;

    $limit = $_REQUEST['limit'];
    $offset = $_REQUEST['offset'];

    $limit = is_numeric($limit)?$limit:50;
    $offset = is_numeric($offset)?$offset:0;
    $max = $offset + $limit;

    $admin = is_admin();
    if ($limit > 100) { $limit = 100 ;}

    if ($handle = opendir($dat_dir)) {
    /* Ceci est la facon correcte de traverser un dossier. */
        $i = 0 ;
        while (false !== ($file = readdir($handle))) {
            if (preg_match("/^profile.[a-z0-9\.]+$/", $file)) {
                if (($i >= $offset) && ($i < $max)) {
                    $i += print_formul_file("$dat_dir/$file",$admin);
                } else {
                    $i++;
                }
                if ($i > $max) {
                    break;
		}
            }
        } 
        closedir($handle);
    }
    if ($offset > 0) {
       $offset = $offset - $limit;
       if ($offset < 0) {
       	  $offset=0;
       }
       echo "<a href='?page=testimonial&offset=$offset&limit=$limit$password'>Prev</a>&nbsp;";
    }
    if ($i >= $max) {
       $offset = $offset + $limit;
       echo "&nbsp;<a href='?page=testimonial&offset=$offset&limit=$limit$password'>Next</a><br>";
    }
}

function export_form($formul)
{
        global $country_lst, $org_type_lst, $org_industry_lst, $os_lst, $catalog_lst, $version_lst;

        $attribs = array('contact_name','email_address', 'org_name','title','website',
                         'hardware_comments','comments',
                         'publish_contact','publish_email', 'publish_orgname','org_size','redundant_setup',
                         'date','visible', 'support','number_dir',
                         'number_fd','number_sd','month_gb','number_files','publish_orgsize','publish_website');
        print "<pre>\n";
        foreach ($attribs as $arr) {
                print "$arr = " . $formul[$arr] . "\n";
        }

        print "orgtype = " . $org_type_lst[$formul['orgtype_id']] . "\n";
        print "orgindustry = " . $org_industry_lst[$formul['orgindustry_id']] . "\n";
        print "bacula_version = " . $version_lst[$formul['bacula_version']] . "\n";
        print "country = " . $country_lst[$formul['country_id']] . "\n";
        print "ostype = " . $os_lst[$formul['ostype_id']] . "\n";
        print "catalog = " . $catalog_lst[$formul['catalog_id']] . "\n";

        print "<pre>\n";
}

function get_file_from_id()
{
    global $dat_dir;
    $id = $_REQUEST['id'];

    if (!ereg('^[a-zA-Z0-9\.]+$',$id)) {
         return(0) ;
    }

    $filename="$dat_dir/profile.$id";

    if (!file_exists($filename)) {
         echo "Can't verify your id";
         return (0);
    }       

    return $filename;
}

function send_email($id, $name, $email)
{
        // Your email address
//        $from = 'kern@sibbald.com';
        $from = 'eric@eb.homelinux.org';

        // The subject
        $subject = "[BACULA] New testimonial";

        // The message
        $message = "Hello, 
You can modify your new testimonial at http://www.bacula.org/en/?page=testimonial&action=Modify&id=$id

Best regards.
";

        mail($email, $subject, $message, "From: Bacula WebMaster <$from>");

        $message = "Hello, 
You can review this testimonial at http://www.bacula.org/en/?page=testimonial&action=Modify&id=$id

Best regards.
";

        
        mail('testimonial@baculasystems.com', $subject, $message, "From: $name <$email>");

        echo "The email has been sent for approval.<br/>";
}

function save_formul($form)
{
        $fp = fopen($form['filename'], 'w'); 
        fwrite($fp, serialize($form));
        fclose($fp);
}

function get_formul()
{
        global $upload_dir;
        global $dat_dir;
        $formul = array();
        $attribs = array('contact_name','email_address', 'org_name');
        foreach ($attribs as $arr) {
                if (!$_REQUEST[$arr]) {
                        echo "Can't get $arr<br/>";
                        return '';
                }
                $formul[$arr] = preg_replace('/[^a-zA-Z0-9!\.?\:\/,;_()@\n -]/', " ", $_REQUEST[$arr]);
        }

        $attribs = array('title','website','hardware_comments','comments')		;
        foreach ($attribs as $arr) {
                $formul[$arr] = preg_replace('/[^a-zA-Z0-9!\.?\:\/,;_()@\n -]/', " ", $_REQUEST[$arr]);
        }
        /* Disallow http:// links into comments field */
        $m = array();
        preg_match('/http:\/\//', $_REQUEST['comments'], $m);
        if (sizeof($m) > 2) {
           return '';
        }
        $m = array();
        preg_match('/http:\/\//', $_REQUEST['hardware_comments'], $m);
        if (sizeof($m) > 2) {
           return '';
        }
        /* Disallow when number of dir too big or > number of fd */
        if (intval($_REQUEST['number_dir']) > 100 || 
            intval($_REQUEST['number_dir']) > intval($_REQUEST['number_fd'])) {
           return '';
        }
        $attribs = array('publish_contact','publish_email', 'publish_orgname', 'orgtype_id', 
                         'orgindustry_id','org_size', 'publish_orgsize','publish_website', 'bacula_version',
                         'country_id','ostype_id', 'redundant_setup','number_fd','number_sd','support',
                         'month_gb','number_files','catalog_id','number_dir');
        foreach ($attribs as $arr) {
                $tmp = $_REQUEST[$arr];
                $tmp = preg_replace("/[,\.]/", "", $tmp);
                if (preg_match("/^[0-9]+$/", $tmp)) {
                        $formul[$arr] = $_REQUEST[$arr];
                }
        }

        $attribs = array('orgtype_id', 'orgindustry_id', 'org_size', 'country_id','bacula_version',
                         'catalog_id', 'ostype_id','number_fd','number_sd', 'number_dir', 'month_gb','number_files');
        foreach ($attribs as $arr) {
                if (!$formul[$arr]) {
                        echo "Can't get $arr<br/>";
                        return '';
                }
        }

        if ($_FILES['org_logo']) {
                $token = uniqid(md5(rand()), true);
                $image = "/upload/$token";

                if (preg_match("/(jpg|jpeg)$/i", $_FILES['org_logo']['name'])) {
                        $image = "$image.jpg";
                } elseif (preg_match("/png$/i", $_FILES['org_logo']['name'])) {
                        $image = "$image.png";
                } elseif (preg_match("/gif$/i", $_FILES['org_logo']['name'])) {
                        $image = "$image.gif";
                } else {
                        $image = '';
                }
                if ($image) {
                        $ret=move_uploaded_file($_FILES['org_logo']['tmp_name'], "$upload_dir/$image");
                        $formul['org_logo'] = $image;
                }
        }
        $formul['date'] = time();
        $formul['visible'] = false;

        return $formul;
}

function form_set_value($formul,$val)
{
 echo "document.getElementById('$val').value = '" . $formul[$val] . "';\n";
}

function form_set_selection($formul,$val)
{
 echo "document.getElementById('${val}_" . $formul[$val] . "').selected =true;\n";
}

function form_set_text($formul,$val)
{
 $temp = $formul[$val];
 $temp = str_replace(array("\n", "\r"), array("\\n",""), $temp);
 echo "document.getElementById('$val').value='" . $temp . "';\n";
}

// passwd file must exist in dat directory
function is_admin()
{
   global $dat_dir;
   $id = $_REQUEST['p'];

   if (!$id) {
       return(false);
   }

   if (!ereg('^[a-zA-Z]+$',$id)) {
       return(false) ;
   }
   
   if (file_exists("$dat_dir/$id")) {
       return true;
   } else {
       return false;
   }
}

function admin_link()
{
   global $password;
   if (is_admin()) {
      $pass = $_REQUEST['p'];
      $waiting = $_REQUEST['waiting'];    
      $password = "&p=$pass";
      print "Admin: ";
      if ($waiting) {
         print "<a href='?page=testimonial&action=Admin$password'> View all</a><br>";
      } else {
         print "<a href='?page=testimonial&action=Admin$password'> View all</a> | ";
         print "<a href='?page=testimonial&action=Admin&waiting=1$password'> View Waiting</a><br>";
      }
	   print "<hr>";
   }
}

function load_formul($filename)
{
   if (!file_exists($filename)) {
           return array();
   }

   if (!filesize($filename) || filesize($filename) > 10*1024*1024) {
       return undef;
   }
   $fp = fopen($filename, 'r');
   $contents = fread ($fp, filesize ($filename));
   fclose ($fp);

   $formul = unserialize($contents);
   if (!is_array($formul)) {
       return undef;
   }

   return $formul;
}

function print_formul_file($filename, $admin) {
    global $password;
    $form = load_formul($filename);
    if (!$form) {
       return 0;
    }
    
    if (!$admin) {
       if (!$form['visible']) {
          return 0;
       }
    } 
    $waiting = $_REQUEST['waiting'];
    if ($admin && $waiting && $form['visible']) {
       return 0;
    }

    $ret = print_formul($form);
    if ($admin) {
       if ($form['visible']) {
          print "<a href=\"?page=testimonial&action=Accept&hide=1$password&id=" . $form['id'] . "\"> Hide </a> | \n";
       } else {
          print "<a href=\"?page=testimonial&action=Accept$password&id=" . $form['id'] . "\"> Accept </a> | \n";
       }
       print "<a href=\"?page=testimonial&action=Modify$password&id=" . $form['id'] . "\"> Modify </a> | \n";
       print "<a href=\"?page=testimonial&action=AdminExport$password&id=" . $form['id'] . "\"> Export </a><br><br>\n";
   }
   return $ret;
}

function dump_sql()
{
   global $dat_dir, $country_lst, $org_type_lst, $org_industry_lst, $os_lst,$catalog_lst, $version_lst;
   if (!is_admin()) {
     return;
   }
?>
   CREATE TABLE dict (lang text, id int, name text, primary key (lang, id));
   CREATE TABLE testimonials (
    contact_name        text,
    email_address       text,
    org_name            text,
    title               text  DEFAULT '',
    website             text  DEFAULT '',
    filename            text  DEFAULT '',
    month_gb            int   DEFAULT 0,
    number_files        bigint DEFAULT 0,
    number_dir          int   DEFAULT 0,
    number_fd           int   DEFAULT 0,
    number_sd           int   DEFAULT 0,
    org_size            int   DEFAULT 0,

    country_id          int,
    ostype_id           int,
    bacula_version      int,
    contry_id           int,
    orgindustry_id      int,
    orgtype_id          int,
    catalog_id          int,

    id                  text,
    publish_contact     int    DEFAULT 0,
    publish_email       int    DEFAULT 0,
    publish_website     int    DEFAULT 0,
    publish_orgname     int    DEFAULT 0,
    publish_orgsize     int    DEFAULT 0,
    redundant_setup     int    DEFAULT 0,
    support             int    DEFAULT 0,
    comments            text   DEFAULT '',
    hardware_comments   text   DEFAULT '', 
    visible             int    DEFAULT 0,
    org_logo            text   DEFAULT '',
    date                int    DEFAULT 0,
    lastmodifed         int    DEFAULT 0
   );

   CREATE VIEW reference AS SELECT contact_name,email_address,org_name,title,website,hardware_comments,comments,
          publish_contact,publish_email,publish_orgname,org_size,publish_orgsize,publish_website,
          redundant_setup,number_fd,number_sd,support,month_gb,number_files,number_dir,date,visible,filename,t.id,
          orgtype.name AS orgtype, 
          orgindustry.name AS orgindustry,
          version.name AS bacula_version,
          country.name AS country,
          ostype.name AS ostype,
          catalog.name AS catalog
   FROM testimonials AS t, dict AS  orgtype, dict AS orgindustry, dict AS version, dict AS country, dict AS ostype,
        dict AS catalog

   WHERE t.orgtype_id = orgtype.id            AND orgtype.lang     = 'en'
     AND t.orgindustry_id = orgindustry.id    AND orgindustry.lang = 'en'
     AND t.bacula_version = version.id AND version.lang = 'en'
     AND t.country_id = country.id            AND country.lang = 'en'
     AND t.ostype_id  = ostype.id             AND ostype.lang = 'en'
     AND t.catalog_id = catalog.id            AND catalog.lang = 'en';


   INSERT INTO dict (lang, id, name) VALUES ('en', 0, 'no');
   INSERT INTO dict (lang, id, name) VALUES ('en', 1, 'yes');
<?
   $a = array($version_lst, $country_lst, $org_type_lst, $org_industry_lst, $os_lst,$catalog_lst);
   foreach ($a as $tab) {
      while(list ($key, $val) = each ($tab))
      {   
          if ($key != 'empty') {
             echo "INSERT INTO dict (lang, id, name) VALUES ('en', $key, '$val');\n";
          }
      }
   }
    if ($handle = opendir($dat_dir)) {
    /* Ceci est la facon correcte de traverser un dossier. */
        while (false !== ($file = readdir($handle))) {
            if (preg_match("/^profile.[a-z0-9\.]+$/", $file)) {
               $tmpv = array();
               $tmpk = array();
               $form = load_formul("$dat_dir/$file");
               while(list ($key, $val) = each ($form))
               {
                   array_push($tmpv, "'$val'");
                   array_push($tmpk, $key);
               }
               echo "INSERT INTO testimonials (";
               print join(",", $tmpk);
               echo ") VALUES (";
               print join(",", $tmpv);
	       echo ");\n";               
            }
        } 
        closedir($handle);
    }
}

function print_formul($formul)
{
   global $country_lst, $org_type_lst, $org_industry_lst, $os_lst,$catalog_lst;

   ?>
   <table width="80%">
   <td>
   <b><? echo ($formul['publish_orgname'])?$formul['org_name']:'N/A' ?></b><br>
   <table>
   <tr><td> Date: </td><td><? echo date('d/m/o', $formul['date']) ?> </td></tr>
   <tr><td> Location: </td><td><? $a = $formul['country_id'] ; echo $country_lst[$a] ?> </td></tr>
   <tr><td> Organization Type: </td><td><? echo $org_type_lst[$formul['orgtype_id']] ?> </td></tr>
   <tr><td> Industry/Function: </td><td><? echo $org_industry_lst[$formul['orgindustry_id']] ?> </td></tr>

   <? if ($formul['publish_orgsize']) { ?>
        <tr><td> Organisation Size: </td><td><? echo $formul['org_size'] ?> </td></tr>
   <? } ?>

   <? if ($formul['publish_website']) { ?>
        <tr><td> Website: </td><td><a href="<? echo $formul['website'] ?>"><? echo $formul['website'] ?></a></td></tr>
   <? } ?>

   <? if ($formul['number_fd'] > 1) { ?>
        <tr><td> Number of fd: </td><td><? echo $formul['number_fd'] ?> </td></tr>
   <? } ?>

   <? if ($formul['month_gb'] > 1) { ?>
        <tr><td> GB/Month: </td><td><? echo $formul['month_gb'] ?> </td></tr>
   <? } ?>

   <tr><td> Comments: </td><td width='650'><i><? echo $formul['comments'] ?> </i></td></tr>
   <? if ($formul['publish_contact']) { ?>
        <tr align='right'><td></td><td><i><? echo $formul['contact_name'] ?> </i></td></tr>
   <? } ?>
<!--
   </table>
   </td>
-->
   <tr><td></td>
   <td>
   <? if ($formul['org_logo']) { ?>
        <a href="<? echo $formul['org_logo'] ?>" ><img width='150' src="<? echo $formul['org_logo'] ?>"></a>
   <? } ?>
   </td>
   </tr> <!-- added -->
   </table>
<?
   return 1;
}

?>
<? require_once("inc/footer.php"); ?>
