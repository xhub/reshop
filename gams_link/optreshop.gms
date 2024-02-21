$ontext
For RESHOP, run "cd global && gams mkopt --solver=reshop"
$offtext

*$setglobal SHORTDOCONLY
$if not set solvername $setglobal solvername reshop
$setlocal HIDDEN_REAL r.(lo mindouble)
$setlocal HIDDEN_INT  i.(lo minint)
$setlocal HIDDEN_BOOL b.(lo 0)
$eolcom //
set g 'RESHOP Option Groups' /
        general        'General options'
    /
    e / auto,'-1',0*100, '>0', n
        conjugate, fenchel, equilibrium
    /,
    f / def Default, lo Lower Bound, up Upper Bound, ref Reference /,
    t / I   Integer
        R   Real
        S   String
        B   Binary
    /
$onempty
    m(*) / /  // eventually we we get them as %system.gamsopt% or so
$offempty

    o 'RESHOP Options' /
     convergence_tolerance           'convergence tolerance of residual'
     cumulative_iteration_limit      'maximum number of cumulative iterations'
     display_empdag_as_png           'display '
     EMPInfoFile                     'EMPinfo file to use (leave empty for GAMS default one)'
     export_models                   'export scalar models via convert'
     major_iteration_limit           'maximum number of major iterations'
     minor_iteration_limit           'maximum number of minor iterations'
     output                          'whether to output logs'
     output_iteration_log            'whether to output iteration logs'
     output_subsolver_log            'whether to output subsolver logs'
     ovf_reformulation                 'scheme used to transform problems with OVF variables'
     ovf_init_new_variables          'initialize the new variables introduce during OVF transformation'
     subsolveropt                    'subsolver option file number'
     time_limit                      'maximum running time in seconds'
     /
$onembedded
    optdata(g,o,t,f) /
      general.(
        convergence_tolerance           .r.(def 1e-6)
        cumulative_iteration_limit      .i.(def 5000)
        EMPInfoFile                     .s.(def '')
        export_models                   .b.(def 0)
        major_iteration_limit           .i.(def 100)
        minor_iteration_limit           .i.(def 1000)
        output                          .i.(def 3)
        output_iteration_log            .b.(def 1)
        output_subsolver_log            .b.(def 1)
        ovf_reformulation                 .s.(def 'equilibrium')
        ovf_init_new_variables          .b.(def 0)
        subsolveropt                    .i.(def 1, lo 1, up 999)
        time_limit                      .i.(def 36000)
      )
    /;
alias (*,u);

$onempty
    set
    oe(o,e) /
      ovf_reformulation.(
         equilibrium               'Nash Equilibrium (or VI formulation)'
         fenchel                   'Fenchel dual (for conic QP)'
         conjugate                 'Conjugate optimization problem'
      )
    /;

set
    publicoe(o,e)     /
      ovf_reformulation.(
        equilibrium                  Equilibrium
        fenchel                      Fenchel
        conjugate                    Conjugate
      )
    /,
    im(*)  immediates recognized  / /
    os(o,*) 'option-synonym mapping'  /
    /
    strlist(o)        / /
    immediate(o,im)   / /
    multilist(o,o)    / /
    dropdefaults(o)   / /
    hidden(*)         / /
    odefault(o)       / /
$offempty
alias (*,u);
