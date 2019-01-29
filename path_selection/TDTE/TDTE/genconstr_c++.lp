\ Model genconstr_c++
\ LP format - for model browsing. Use MPS format to capture full model detail.
Maximize
  0 Clause0 + 0 Clause1 + 0 Clause2 + 0 Clause3 + 0 Clause4 + 0 Clause5
   + 0 Clause6 + 0 Clause7 + Obj0 + Obj1
Subject To
 CNSTR_X0: X0 + notX0 = 1
 CNSTR_X1: X1 + notX1 = 1
 CNSTR_X2: X2 + notX2 = 1
 CNSTR_X3: X3 + notX3 = 1
 CNSTR_Obj1: Obj1 = 1 -> Clause0 + Clause1 + Clause2 + Clause3 + Clause4
   + Clause5 + Clause6 + Clause7 >= 4
Bounds
Binaries
 X0 notX0 X1 notX1 X2 notX2 X3 notX3 Clause0 Clause1 Clause2 Clause3
 Clause4 Clause5 Clause6 Clause7 Obj0 Obj1
General Constraints
 CNSTR_Clause0: Clause0 = OR ( X0 , notX1 , X2 )
 CNSTR_Clause1: Clause1 = OR ( X1 , notX2 , X3 )
 CNSTR_Clause2: Clause2 = OR ( X2 , notX3 , X0 )
 CNSTR_Clause3: Clause3 = OR ( X3 , notX0 , X1 )
 CNSTR_Clause4: Clause4 = OR ( notX0 , notX1 , X2 )
 CNSTR_Clause5: Clause5 = OR ( notX1 , notX2 , X3 )
 CNSTR_Clause6: Clause6 = OR ( notX2 , notX3 , X0 )
 CNSTR_Clause7: Clause7 = OR ( notX3 , notX0 , X1 )
 CNSTR_Obj0: Obj0 = MIN ( Clause0 , Clause1 , Clause2 , Clause3 , Clause4 ,
   Clause5 , Clause6 , Clause7 )
End
