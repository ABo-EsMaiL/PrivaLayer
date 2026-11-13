# PrivaLayer — Personal Workspace

نسخة أولية قابلة للبناء على Windows 10/11 باستخدام C++20 وWin32 API وMicrosoft WebView2.

## الموجود في النسخة الحالية

- واجهة Windows باسم وهوية PrivaLayer وأيقونة مخصصة.
- أقسام Overview وNotes وBrowser وAI Assistant وWorkspace وSettings.
- Notes تحفظ محليًا في `%LOCALAPPDATA%\PrivaLayer\notes.txt`.
- متصفح مدمج باستخدام WebView2، مع مجلد بيانات مستقل لحفظ جلسات المواقع التي تسمح بتسجيل الدخول من WebView2.
- زر يفتح Windows Snipping Tool عبر `Win + Shift + S`، لتختار بنفسك منطقة الشاشة.
- زر يستورد صورة الـClipboard ويحفظها بصيغة PNG في `Pictures\PrivaLayer`.
- زر يفتح Google AI Studio في Microsoft Edge؛ بعد فتحه الصق الصورة يدويًا باستخدام `Ctrl + V`.
- إعدادات شفافية النافذة، إبقاؤها أعلى النوافذ، وضع تمرير النقرات، وثلاثة Themes.
- اختصارات عامة:
  - `Ctrl + Alt + Space`: إظهار/إخفاء نافذة البرنامج.
  - `Ctrl + Alt + I`: التبديل بين الوضع التفاعلي وتمرير النقرات.
  - `Ctrl + Alt + 1`: فتح أداة القص في Windows.

## البناء

المتطلبات:

1. Windows 10 إصدار 2004 أو أحدث، أو Windows 11.
2. Visual Studio 2022 مع workload باسم **Desktop development with C++**.
3. CMake 3.24 أو أحدث.
4. اتصال إنترنت أثناء أول Configure لتنزيل WebView2 SDK من NuGet.
5. وجود Microsoft Edge WebView2 Runtime على الجهاز لتشغيل المتصفح المدمج.

افتح PowerShell داخل مجلد المشروع ثم نفّذ:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\build.ps1
```

أو بالأوامر المباشرة:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

المفترض أن يكون الملف الناتج:

```text
build\Release\PrivaLayer.exe
```

للتشغيل:

```powershell
.\run.ps1
```

## التصوير وإرسال الصورة إلى AI Studio

1. افتح قسم AI Assistant واضغط **Start screen snip**.
2. حدد المنطقة بنفسك من واجهة Windows Snipping Tool.
3. ارجع إلى PrivaLayer واضغط **Import clipboard image**.
4. سيحفظ البرنامج PNG محليًا، مع إبقاء صورة القص في Clipboard.
5. اضغط **Open AI Studio in Edge**، ثم الصق الصورة باستخدام `Ctrl + V`.

لا ينفذ البرنامج تصويرًا دوريًا أو رفعًا في الخلفية. اختيار المنطقة والإرسال إلى AI Studio يتطلبان إجراءً مباشرًا منك.

## حدود مهمة قبل الاستخدام

- هذه النسخة لا تستخدم `WDA_EXCLUDEFROMCAPTURE` ولا تحاول إخفاء واجهة البرنامج من برامج الاجتماعات أو التسجيل. ستظهر النافذة في مشاركة الشاشة العادية إذا كانت ظاهرة على الشاشة المشتركة.
- الشفافية وPass-through لا تعنيان إخفاء النافذة من Screen Sharing.
- المتصفح المدمج WebView2 له Profile مستقل عن Edge. بعض المواقع، ومنها تدفقات تسجيل الدخول الخاصة بخدمات Google، قد تمنع تسجيل الدخول داخل WebView2. لذلك يوجد زر لفتح AI Studio في Edge العادي.
- زر الصورة يستعمل أداة القص الرسمية في Windows ويحتاج اختيارًا يدويًا؛ لا يقوم بالتقاط الشاشة سرًا.
- AI Assistant في هذه النسخة هو مساحة تجهيز Prompt وفتح AI Studio، وليس اتصال API مستقلًا.
- المشروع مصدر C++ قابل للبناء، ولم يتم إنتاج EXE داخل بيئة التطوير الحالية لأنها ليست Windows ولا تحتوي على MSVC. يلزم تنفيذ أوامر البناء على جهاز Windows بالمواصفات أعلاه.

## أماكن الملفات المحلية

- Notes وبيانات المتصفح: `%LOCALAPPDATA%\PrivaLayer\`
- الصور المحفوظة: `%USERPROFILE%\Pictures\PrivaLayer\`

