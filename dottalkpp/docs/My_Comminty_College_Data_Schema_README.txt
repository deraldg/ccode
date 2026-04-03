My Community College  FoxPro 2.6a Compatible DBFs (dBase III format)
---------------------------------------------------------------------
Tables (record counts approx.):
  TEACHERS  (20)
  STUDENTS  (200)
  COURSES   (100)
  CLASSES   (200)
  ENROLL    (~800)
  DEPT      (10)
  BUILDING  (8)
  ROOMS     (30)
  TERMS     (3)
  MAJORS    (12)
  STUD_MAJ  (200)
  TASSIGN   (200)

Notes:
- Files are written as dBase III (version 0x03) with no memo fields for
  maximum compatibility with FoxPro 2.6a.
- Field names are <=10 chars and uppercase; dates are YYYYMMDD.
- Suggested indexes (create in FoxPro):
    STUDENTS:  INDEX ON SID TAG SID, INDEX ON UPPER(LNAME+FNAME) TAG NAME
    TEACHERS:  INDEX ON TID TAG TID, INDEX ON DEPT_ID TAG DEPT
    COURSES:   INDEX ON CID TAG CID, INDEX ON DEPT_ID TAG DEPT
    CLASSES:   INDEX ON CLS_ID TAG CLS, INDEX ON TERM+CID+STR(SEC,2) TAG TCSEC
    ENROLL:    INDEX ON SID TAG SID, INDEX ON CLS_ID TAG CLS
    ROOMS:     INDEX ON ROOM TAG ROOM
    MAJORS:    INDEX ON MAJOR TAG MAJOR
    DEPT:      INDEX ON DEPT_ID TAG DEPT
    TERMS:     INDEX ON TERM TAG TERM
    STUD_MAJ:  INDEX ON SID TAG SID
    TASSIGN:   INDEX ON CLS_ID TAG CLS

Sample relationships:
  STUDENTS.SID  -> ENROLL.SID, STUD_MAJ.SID
  COURSES.CID   -> CLASSES.CID
  CLASSES.CLS_ID-> ENROLL.CLS_ID, TASSIGN.CLS_ID
  TEACHERS.TID  -> CLASSES.TID, TASSIGN.TID
  DEPT.DEPT_ID  -> TEACHERS.DEPT_ID, COURSES.DEPT_ID, MAJORS.DEPT_ID
  BUILDING/ROOMS linked by ROOMS.BLDG
