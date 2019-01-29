NAME genconstr_c++
* Max problem is converted into Min one
ROWS
 N  OBJ
 E  CNSTR_X0
 E  CNSTR_X1
 E  CNSTR_X2
 E  CNSTR_X3
 G  CNSTR_Obj1
COLUMNS
    MARKER    'MARKER'                 'INTORG'
    X0        CNSTR_X0  1
    notX0     CNSTR_X0  1
    X1        CNSTR_X1  1
    notX1     CNSTR_X1  1
    X2        CNSTR_X2  1
    notX2     CNSTR_X2  1
    X3        CNSTR_X3  1
    notX3     CNSTR_X3  1
    Clause0   OBJ       0
    Clause0   CNSTR_Obj1  1
    Clause1   OBJ       0
    Clause1   CNSTR_Obj1  1
    Clause2   OBJ       0
    Clause2   CNSTR_Obj1  1
    Clause3   OBJ       0
    Clause3   CNSTR_Obj1  1
    Clause4   OBJ       0
    Clause4   CNSTR_Obj1  1
    Clause5   OBJ       0
    Clause5   CNSTR_Obj1  1
    Clause6   OBJ       0
    Clause6   CNSTR_Obj1  1
    Clause7   OBJ       0
    Clause7   CNSTR_Obj1  1
    Obj0      OBJ       -1
    Obj1      OBJ       -1
    MARKER    'MARKER'                 'INTEND'
RHS
    RHS1      CNSTR_X0  1
    RHS1      CNSTR_X1  1
    RHS1      CNSTR_X2  1
    RHS1      CNSTR_X3  1
    RHS1      CNSTR_Obj1  4
BOUNDS
 BV BND1      X0      
 BV BND1      notX0   
 BV BND1      X1      
 BV BND1      notX1   
 BV BND1      X2      
 BV BND1      notX2   
 BV BND1      X3      
 BV BND1      notX3   
 BV BND1      Clause0 
 BV BND1      Clause1 
 BV BND1      Clause2 
 BV BND1      Clause3 
 BV BND1      Clause4 
 BV BND1      Clause5 
 BV BND1      Clause6 
 BV BND1      Clause7 
 BV BND1      Obj0    
 BV BND1      Obj1    
INDICATORS
 IF CNSTR_Obj1  Obj1      1
GENCONS
 OR CNSTR_Clause0
    Clause0
    X0
    notX1
    X2
 OR CNSTR_Clause1
    Clause1
    X1
    notX2
    X3
 OR CNSTR_Clause2
    Clause2
    X2
    notX3
    X0
 OR CNSTR_Clause3
    Clause3
    X3
    notX0
    X1
 OR CNSTR_Clause4
    Clause4
    notX0
    notX1
    X2
 OR CNSTR_Clause5
    Clause5
    notX1
    notX2
    X3
 OR CNSTR_Clause6
    Clause6
    notX2
    notX3
    X0
 OR CNSTR_Clause7
    Clause7
    notX3
    notX0
    X1
 MIN CNSTR_Obj0
    Obj0
    Clause0
    Clause1
    Clause2
    Clause3
    Clause4
    Clause5
    Clause6
    Clause7
ENDATA
