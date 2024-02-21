SCALAR kappa;
kappa = 1;
SCALAR epsilon;
epsilon = 1;
SCALAR lambda;
lambda = 1;

$if not set qs_function $setglobal qs_function "huber"
$if not set kappa_val $setglobal kappa_val 1
$if not set lambda_val $setglobal lambda_val 1
$if not set epsilon_val $setglobal epsilon_val 1

$IFTHEN %qs_function%=="huber"
$setglobal lambda_str ""
$setglobal epsilon_str ""
$setglobal kappa_str "kappa"
$ENDIF

$IFTHEN %qs_function%=="huber_scaled"
$setglobal lambda_str ""
$setglobal epsilon_str ""
$setglobal kappa_str "kappa"
$ENDIF

$IFTHEN %qs_function%=="l1"
$setglobal lambda_str ""
$setglobal epsilon_str ""
$setglobal kappa_str ""
$ENDIF

$IFTHEN %qs_function%=="l2"
$setglobal lambda_str ""
$setglobal epsilon_str ""
$setglobal kappa_str ""
$ENDIF

$IFTHEN %qs_function%=="elastic_net"
$setglobal lambda_str "lambda"
$setglobal epsilon_str ""
$setglobal kappa_str ""
$ENDIF

$IFTHEN %qs_function%=="hinge"
$setglobal lambda_str ""
$setglobal epsilon_str "epsilon"
$setglobal kappa_str ""
$ENDIF

$IFTHEN %qs_function%=="vapnik"
$setglobal lambda_str ""
$setglobal epsilon_str "epsilon"
$setglobal kappa_str ""
$ENDIF

$IFTHEN %qs_function%=="soft_hinge"
$setglobal lambda_str ""
$setglobal epsilon_str "epsilon"
$setglobal kappa_str "kappa"
$ENDIF

$IFTHEN %qs_function%=="hubnik"
$setglobal lambda_str ""
$setglobal epsilon_str "epsilon"
$setglobal kappa_str "kappa"
$ENDIF

$IFTHEN %qs_function%=="soft_hinge_scaled"
$setglobal lambda_str ""
$setglobal epsilon_str "epsilon"
$setglobal kappa_str "kappa"
$ENDIF

$IFTHEN %qs_function%=="hubnik_scaled"
$setglobal lambda_str ""
$setglobal epsilon_str "epsilon"
$setglobal kappa_str "kappa"
$ENDIF


