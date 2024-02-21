ReSHOP is a library that models optimization problems with nonclassical and rich structures and performs transformations on them. Examples include Nash Equilibrium, Problems with Equilibrium Constraints, Optimal Value Function.
The term *Extended Mathematical Programming* (EMP) subsumes all of those cases. ReSHOP aims at solving EMP problems applying reformulations into forms amenable to computations.


It supports the following EMP problems:
- (Generalized) Nash Equilibrium Problem and Multiple Objectives Problem with Equilibrium Constraints (MOPEC)
- Optimal Value Functions, a concept that subsumes regularizers, coherent risk measures (like CVaR)


ReSHOP can be used from GAMS, Python, and Julia, as well as a standalone library. In GAMS it relies on annotations to define the EMP structure, whereas in Python and Julia, there is a programmatic approach to the EMP problem definition.
