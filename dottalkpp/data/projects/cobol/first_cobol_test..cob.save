       IDENTIFICATION DIVISION.
       PROGRAM-ID. FIRST-COBOL-TEST.

       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       FILE-CONTROL.
           SELECT STUDENT-FILE
               ASSIGN TO "D:\code\ccode\dottalkpp\data\students_ro.dat"
               ORGANIZATION IS LINE SEQUENTIAL.

       DATA DIVISION.
       FILE SECTION.
       FD  STUDENT-FILE.
       01  STUDENT-REC.
           05 SID-FLD        PIC X(8).
           05 LNAME-FLD      PIC X(20).
           05 FNAME-FLD      PIC X(15).
           05 DOB-FLD        PIC X(8).
           05 GENDER-FLD     PIC X(1).
           05 MAJOR-FLD      PIC X(4).
           05 ENROLL-D-FLD   PIC X(8).
           05 GPA-FLD        PIC X(5).
           05 EMAIL-FLD      PIC X(40).

       WORKING-STORAGE SECTION.
       01  EOF-FLAG          PIC X VALUE "N".
       01  REC-COUNT         PIC 9(7) VALUE 0.

       PROCEDURE DIVISION.
           OPEN INPUT STUDENT-FILE

           PERFORM UNTIL EOF-FLAG = "Y"
               READ STUDENT-FILE
                   AT END
                       MOVE "Y" TO EOF-FLAG
                   NOT AT END
                       ADD 1 TO REC-COUNT
                       DISPLAY SID-FLD "  "
                               LNAME-FLD "  "
                               FNAME-FLD "  "
                               GPA-FLD
               END-READ
           END-PERFORM

           CLOSE STUDENT-FILE
           DISPLAY "RECORDS READ: " REC-COUNT
           STOP RUN.