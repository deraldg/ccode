# ANSI-safe demo
from pydottalk import Session

# CLI backend (works today). Provide the exe path.
sess = Session(workdir=r"C:\Users\deral\code\ccode\data",
               exe=r"C:\Users\deral\code\ccode\build\Release\dottalkpp.exe")

csv_path = sess.export_csv("STUDENTS",
                           fields=["STUDENT_ID","LAST_NAME","FIRST_NAME","GPA"],
                           for_clause='last_name = "Grimwood"')
print("CSV written:", csv_path)

print("--- FIELDS ---")
print(sess.fields("STUDENTS"))

# Later: native backend (flip CMake option DTX_BACKEND_NATIVE=ON)
# sess_native = Session(workdir=r"C:\...\data")
# csv_path2 = sess_native.export_csv("STUDENTS", fields=["STUDENT_ID","LAST_NAME"])
# print(csv_path2)
