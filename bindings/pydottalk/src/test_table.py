from table import Table

t = Table(r"D:\code\ccode\dottalkpp\data\xdbf\students.dbf")
t.open()

print("fields:", t.field_names())
print("count :", t.rec_count())

t.top()
print("first :", t.read())

t.skip(1)
print("next  :", t.read())

print("iter sample:")
for r in t.records(limit=3):
    print(" ", r)

t.close()