*===============================================================
* make_test_dbfs.prg  (FoxPro 2.x / dBASE IV memo = .DBT)
* Creates & populates sample DBFs with memo fields.
*===============================================================
SET SAFETY OFF
SET EXCLUSIVE ON
SET DELETED ON
SET TALK OFF
RANDOMIZE

* --- helper data pools ---
PUBLIC aFirst, aLast, aCity, aDept, aCourse, aBook
DIMENSION aFirst[12]
DIMENSION aLast[12]
DIMENSION aCity[8]
DIMENSION aDept[8]
DIMENSION aCourse[10]
DIMENSION aBook[10]

aFirst = "Alice,Bob,Charlie,Diana,Eve,Frank,Grace,Heidi,Ivan,Judy,Mallory,Peggy"
aLast  = "Grimwood,Smith,Johnson,Williams,Brown,Jones,Garcia,Miller,Davis,Wilson,Moore,Taylor"
aCity  = "Eugene,Portland,Medford,Bend,Corvallis,Ashland,Hillsboro,Springfield"
aDept  = "MATH,CS,ENG,HIST,BIO,PHYS,ART,ECON"
aCourse= "Algebra,DataStruct,CompLit,USHistory,GenBio,PhysI,DrawingI,Macro,DBDesign,NetBasics"
aBook  = "Algorithms,Databases,System Design,Networking,Statistics,Linear Algebra,OS Concepts,Compilers,AI Basics,Discrete Math"

* Convert comma lists into arrays
= _FillArray(@aFirst)
= _FillArray(@aLast)
= _FillArray(@aCity)
= _FillArray(@aDept)
= _FillArray(@aCourse)
= _FillArray(@aBook)

* --- build everything ---
= MakeStudents(25)
= MakeTeachers(12)
= MakeCourses(14)
= MakeClasses(30)
= MakeBook(10)
= MakeJoinTable(40)

? "Done. Created and populated STUDENTS, TEACHERS, COURSES, CLASSES, BOOK, JOIN_TABLE."

RETURN

*-------------------------
PROCEDURE _FillArray
PARAMETERS aRef
LOCAL i, c, n
c = aRef
n = OCCURS(",", c) + 1
DIMENSION aRef[n]
FOR i = 1 TO n
  aRef[i] = ALLTRIM(GETWORDNUM(c, i, ","))
ENDFOR
RETURN

*-------------------------
PROCEDURE RandItem
PARAMETERS aRef
LOCAL n
n = ALEN(aRef, 1)
RETURN aRef[INT(RAND()*n)+1]

*-------------------------
FUNCTION RandBool
RETURN IIF(RAND() >= 0.5, .T., .F.)

*-------------------------
FUNCTION RandDatePastYears
PARAMETERS nYears
LOCAL days
days = INT(RAND()*(nYears*365))
RETURN DATE() - days

*-------------------------
FUNCTION RandMoney
PARAMETERS lo, hi
RETURN ROUND(lo + RAND() * (hi - lo), 2)

*-------------------------
FUNCTION RandInt
PARAMETERS lo, hi
RETURN lo + INT(RAND() * (hi - lo + 1))

*-------------------------
PROCEDURE FillMemo
PARAMETERS mTarget
* Produce 1–3 short paragraphs
LOCAL i, paras, p
paras = ""
FOR i = 1 TO (1 + INT(RAND()*3))
  p = "Project memo text on iteration, indexing, integration, testing, and workflow. " + ;
      "This record seeded for demo purposes; storage via DBT memo."
  paras = paras + p
  IF i < 3
    paras = paras + CHR(13)+CHR(10)+CHR(13)+CHR(10)
  ENDIF
ENDFOR
REPLACE &mTarget WITH paras
RETURN

*===============================================================
* STUDENTS
*===============================================================
PROCEDURE MakeStudents
PARAMETERS nRows
IF FILE("STUDENTS.DBF")
  ERASE STUDENTS.DBF
  IF FILE("STUDENTS.DBT")
    ERASE STUDENTS.DBT
  ENDIF
ENDIF
CREATE TABLE STUDENTS ( ;
  STUDENT_ID N(6,0), ;
  LAST_NAME  C(20), ;
  FIRST_NAME C(15), ;
  CITY       C(20), ;
  BIRTHDATE  D, ;
  GPA        N(3,2), ;
  ACTIVE     L, ;
  NOTES      M )

USE STUDENTS EXCLUSIVE
FOR i = 1 TO nRows
  APPEND BLANK
  REPLACE STUDENT_ID WITH 100000 + i
  REPLACE LAST_NAME  WITH PADR(RandItem(aLast), 20)
  REPLACE FIRST_NAME WITH PADR(RandItem(aFirst), 15)
  REPLACE CITY       WITH PADR(RandItem(aCity), 20)
  REPLACE BIRTHDATE  WITH DATE() - RandInt(18*365, 80*365)
  REPLACE GPA        WITH ROUND( 2.00 + RAND()*2.00, 2 )
  REPLACE ACTIVE     WITH RandBool()
  = FillMemo("NOTES")
ENDFOR
PACK
USE
RETURN

*===============================================================
* TEACHERS
*===============================================================
PROCEDURE MakeTeachers
PARAMETERS nRows
IF FILE("TEACHERS.DBF")
  ERASE TEACHERS.DBF
  IF FILE("TEACHERS.DBT")
    ERASE TEACHERS.DBT
  ENDIF
ENDIF
CREATE TABLE TEACHERS ( ;
  TEACHER_ID N(6,0), ;
  LAST_NAME  C(20), ;
  FIRST_NAME C(15), ;
  HIRE_DATE  D, ;
  SALARY     N(9,2), ;
  ACTIVE     L, ;
  NOTES      M )

USE TEACHERS EXCLUSIVE
FOR i = 1 TO nRows
  APPEND BLANK
  REPLACE TEACHER_ID WITH 200000 + i
  REPLACE LAST_NAME  WITH PADR(RandItem(aLast), 20)
  REPLACE FIRST_NAME WITH PADR(RandItem(aFirst), 15)
  REPLACE HIRE_DATE  WITH RandDatePastYears(20)
  REPLACE SALARY     WITH RandMoney(45000, 125000)
  REPLACE ACTIVE     WITH RandBool()
  = FillMemo("NOTES")
ENDFOR
PACK
USE
RETURN

*===============================================================
* COURSES
*===============================================================
PROCEDURE MakeCourses
PARAMETERS nRows
IF FILE("COURSES.DBF")
  ERASE COURSES.DBF
  IF FILE("COURSES.DBT")
    ERASE COURSES.DBT
  ENDIF
ENDIF
CREATE TABLE COURSES ( ;
  COURSE_ID C(8), ;
  TITLE     C(60), ;
  DEPT      C(8), ;
  CREDITS   N(2,0), ;
  NOTES     M )

USE COURSES EXCLUSIVE
FOR i = 1 TO nRows
  APPEND BLANK
  REPLACE COURSE_ID WITH PADR( "C" + RIGHT("0000"+LTRIM(STR(i)),4), 8)
  REPLACE TITLE     WITH PADR(RandItem(aCourse), 60)
  REPLACE DEPT      WITH PADR(RandItem(aDept), 8)
  REPLACE CREDITS   WITH RandInt(1, 5)
  = FillMemo("NOTES")
ENDFOR
PACK
USE
RETURN

*===============================================================
* CLASSES  (sections of courses taught by teachers)
*===============================================================
PROCEDURE MakeClasses
PARAMETERS nRows
IF FILE("CLASSES.DBF")
  ERASE CLASSES.DBF
  IF FILE("CLASSES.DBT")
    ERASE CLASSES.DBT
  ENDIF
ENDIF
CREATE TABLE CLASSES ( ;
  CLASS_ID   N(6,0), ;
  COURSE_ID  C(8), ;
  TEACHER_ID N(6,0), ;
  TERM       C(6), ;
  ROOM       C(10), ;
  START_DATE D, ;
  END_DATE   D, ;
  NOTES      M )

USE CLASSES EXCLUSIVE
FOR i = 1 TO nRows
  LOCAL nCredits, dStart, dEnd
  APPEND BLANK
  REPLACE CLASS_ID   WITH 300000 + i
  REPLACE COURSE_ID  WITH PADR( "C" + RIGHT("0000"+LTRIM(STR(RandInt(1,14))),4), 8)
  REPLACE TEACHER_ID WITH 200000 + RandInt(1,12)
  REPLACE TERM       WITH PADR( IIF(RAND()>.5,"FA25","SP25"), 6)
  REPLACE ROOM       WITH PADR("RM-" + RIGHT("00"+LTRIM(STR(RandInt(1,40))),2), 10)
  dStart = DATE(2025, IIF(RAND()>.5, 9, 1), RandInt(1, 28))
  dEnd   = dStart + RandInt(60, 120)
  REPLACE START_DATE WITH dStart
  REPLACE END_DATE   WITH dEnd
  = FillMemo("NOTES")
ENDFOR
PACK
USE
RETURN

*===============================================================
* BOOK
*===============================================================
PROCEDURE MakeBook
PARAMETERS nRows
IF FILE("BOOK.DBF")
  ERASE BOOK.DBF
  IF FILE("BOOK.DBT")
    ERASE BOOK.DBT
  ENDIF
ENDIF
CREATE TABLE BOOK ( ;
  BOOK_ID   N(6,0), ;
  TITLE     C(60), ;
  AUTHOR    C(40), ;
  PUBLISHED D, ;
  ISBN      C(13), ;
  NOTES     M )

USE BOOK EXCLUSIVE
FOR i = 1 TO nRows
  LOCAL cAuth
  APPEND BLANK
  cAuth = PADR(RandItem(aLast) + ", " + RandItem(aFirst), 40)
  REPLACE BOOK_ID   WITH 400000 + i
  REPLACE TITLE     WITH PADR(RandItem(aBook), 60)
  REPLACE AUTHOR    WITH cAuth
  REPLACE PUBLISHED WITH RandDatePastYears(30)
  REPLACE ISBN      WITH PADR( "978" + RIGHT("000000000"+LTRIM(STR(RandInt(1,999999999))),9), 13)
  = FillMemo("NOTES")
ENDFOR
PACK
USE
RETURN

*===============================================================
* JOIN_TABLE (enrollments: student -> class with grade)
*===============================================================
PROCEDURE MakeJoinTable
PARAMETERS nRows
IF FILE("JOIN_TABLE.DBF")
  ERASE JOIN_TABLE.DBF
  IF FILE("JOIN_TABLE.DBT")
    ERASE JOIN_TABLE.DBT
  ENDIF
ENDIF
CREATE TABLE JOIN_TABLE ( ;
  STUDENT_ID N(6,0), ;
  CLASS_ID   N(6,0), ;
  GRADE      C(2), ;
  ENROLLED   D, ;
  NOTES      M )

USE JOIN_TABLE EXCLUSIVE
FOR i = 1 TO nRows
  LOCAL g
  APPEND BLANK
  REPLACE STUDENT_ID WITH 100000 + RandInt(1,25)
  REPLACE CLASS_ID   WITH 300000 + RandInt(1,30)
  g = SUBSTR("A B C D F", 1 + 2*RandInt(0,4), 1)
  REPLACE GRADE      WITH g
  REPLACE ENROLLED   WITH RandDatePastYears(2)
  = FillMemo("NOTES")
ENDFOR
PACK
USE
RETURN
