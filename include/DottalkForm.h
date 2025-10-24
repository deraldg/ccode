#include "DottalkForm.h"
using namespace DottalkGui;
[System::STAThreadAttribute]
int main(array<System::String ^> ^args)
{
    Application::EnableVisualStyles();
    Application::SetCompatibleTextRenderingDefault(false);
    Application::Run(gcnew DottalkForm());
    return 0;
}