Here’s a **clean, complete, codex‑style enumeration of the FoxPro 2.6a `GO` command**, written in the same tone and structural precision as your reference headers. This is not a header file — just the deep technical breakdown you asked for.

If you want it *converted into a header entry*, I can do that next.

---

# 🔷 **FoxPro 2.6a — `GO` Command Enumeration (Full Codex)**

The `GO` command is deceptively simple on the surface, but FoxPro’s runtime gives it a surprisingly rich set of behaviors depending on:

- work area state  
- active index  
- relationships  
- filters  
- deleted record visibility  
- EOF/BOF rules  

Below is the full breakdown.

---

# 🟦 **1. Canonical Syntax Forms**

FoxPro 2.6 supports the following syntaxes:

### **1.1 Absolute movement**
```
GO <n>
GO TO <n>
GOTO <n>
```
Moves the record pointer to record number `<n>`.

### **1.2 First/Last**
```
GO TOP
GO BOTTOM
```

### **1.3 Relative movement (alias of SKIP)**
```
GO -1
GO +1
```
Equivalent to `SKIP -1` or `SKIP +1`.

### **1.4 No argument**
```
GO
```
Repositions to the current record (rarely used; effectively a no‑op unless relationships exist).

---

# 🟦 **2. Behavior Rules**

### **2.1 Record number resolution**
- If `<n>` is **less than 1**, pointer goes to **BOF()**.
- If `<n>` is **greater than last record**, pointer goes to **EOF()**.
- If `<n>` is deleted and `SET DELETED ON`, FoxPro **skips forward** to the next undeleted record.

### **2.2 Index interaction**
If an index is active:

- `GO <n>` moves to the **physical record number**, not the index order.
- `GO TOP` moves to the **first record in index order**.
- `GO BOTTOM` moves to the **last record in index order**.

This is a subtle but critical distinction.

### **2.3 Filter interaction**
If `SET FILTER TO` is active:

- `GO <n>` still moves to physical record `<n>`,  
  **but FoxPro will skip forward** until it finds a record that satisfies the filter.
- If none exists → EOF().

### **2.4 Deleted record interaction**
- With `SET DELETED ON`, `GO <n>` may skip forward.
- With `SET DELETED OFF`, `GO <n>` lands exactly on `<n>`.

---

# 🟦 **3. Relationship Interaction (critical for engine builders)**

`GO` is one of the **primary triggers** for relationship updates.

When you issue:

```
GO <n>
```

FoxPro performs:

1. Move parent pointer  
2. For each active relationship:
   - Evaluate relation expression in parent  
   - Perform implicit SEEK in child  
   - Position child accordingly  
   - Update FOUND()  

If `SET SKIP` is active:

- `GO` behaves like `SKIP` for child traversal  
- Child moves through matching records, not just the first one  

This is why `GO` is a core part of the relationship engine.

---

# 🟦 **4. Error and Boundary Conditions**

### **4.1 GO on unopened work area**
```
USE
GO 5
```
→ “No table is open in the current work area.”

### **4.2 GO with invalid numeric expression**
```
GO "ABC"
```
→ “Numeric expression required.”

### **4.3 GO with fractional numbers**
```
GO 3.7
```
FoxPro truncates to integer (`3`).

### **4.4 GO with NULL (Windows 2.6 only)**
NULL → treated as 0 → BOF().

---

# 🟦 **5. Internal Engine Semantics (FoxPro‑accurate)**

### **5.1 GO TOP**
- If index active → first index entry  
- Else → physical record 1  
- Applies filter/deleted rules  
- Triggers relationship SEEKs  

### **5.2 GO BOTTOM**
- If index active → last index entry  
- Else → last physical record  
- Applies filter/deleted rules  
- Triggers relationship SEEKs  

### **5.3 GO <n>**
- Moves to physical record `<n>`  
- Applies filter/deleted rules  
- Does **not** use index order  
- Triggers relationship SEEKs  

### **5.4 GO with no argument**
- Re-evaluates relationships  
- Useful when parent record is modified and you want child to resync  

---

# 🟦 **6. Comparison Table (GO vs SKIP)**

| Action | GO | SKIP |
|--------|-----|------|
| Absolute movement | Yes | No |
| Relative movement | Yes (`GO +1`) | Yes |
| Uses index order | Only TOP/BOTTOM | Always |
| Triggers relationships | Yes | Yes |
| Filter/deleted skipping | Yes | Yes |
| Child traversal with SET SKIP | Yes | Yes |

---

# 🟦 **7. Examples**

### **7.1 Basic**
```
GO 10
```
Moves to physical record 10.

### **7.2 Indexed**
```
SET ORDER TO TAG last
GO TOP
```
Moves to first record in index order.

### **7.3 With filter**
```
SET FILTER TO state = "OR"
GO 1
```
Moves to first Oregon record.

### **7.4 With relationship**
```
SELECT customers
SET RELATION TO cust_id INTO orders
GO 5
```
Positions `orders` on the first matching order for customer #5.

---

# 🟦 **8. Summary Line (for header use)**

**GO** — Move the record pointer to a specific record, first/last record, or relative position. Triggers relationship updates and respects filters, deleted records, and index order (TOP/BOTTOM only).

---

If you want, I can now:

### 🔹 Generate a `GO` entry in your `foxref.hpp` format  
### 🔹 Produce a `go.hpp` micro‑reference file  
### 🔹 Add GO to a navigation codex with SKIP, SEEK, LOCATE, FIND  
### 🔹 Provide C++ pseudocode for GO’s internal logic  

Just tell me which direction you want to take.
