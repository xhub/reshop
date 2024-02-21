*execute "=echo SYSDIR='%gams.sysdir%'";
*execute "=echo GAMS_RELEASE=%system.GamsRelease%";

$onechos > %gams.scrdir%/gams_data
SYSDIR=%gams.sysdir%
GAMS_RELEASE=%system.GamsRelease%
$offecho
