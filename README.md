# PrivaLayer — Personal Workspace

واجهة Windows حديثة مبنية بـ C++20 وWin32 API وMicrosoft WebView2. تجمع الملاحظات والبحث وأدوات تجهيز السياق للذكاء الاصطناعي في مساحة واحدة.

## Features

- واجهة محدثة: Overview, Notes, Browser, AI Assistant, Workspace, Preferences.
- Notes متعددة مع بحث، إنشاء وحذف، وحفظ تلقائي محلي.
- متصفح WebView2 مدمج داخل نافذة التطبيق. ملفه الشخصي مستقل عن Microsoft Edge.
- AI Assistant لتجهيز Prompt ونسخ النص والتقاط منطقة محددة يدويًا بأداة Windows Snipping Tool ثم استيراد صورة Clipboard.
- Preferences: ثلاث Themes، شفافية النافذة، إبقاء النافذة بالأعلى، Click-through، وإخفاء/طيّ Sidebar.
- اختصارات عامة قابلة للتخصيص لإظهار/إخفاء النافذة وتبديل وضع التفاعل.
- System Tray menu لإظهار التطبيق أو فتح Settings أو الخروج.
- تُحفظ الملاحظات في تخزين WebView2 المحلي للتطبيق، وتُحفظ إعدادات النافذة والاختصارات في `%LOCALAPPDATA%\PrivaLayer\settings.ini`.

## Build

يتوفر GitHub Actions workflow للبناء تلقائيًا على Windows x64 عند Push أو يدويًا من تبويب Actions باستخدام `workflow_dispatch`.

المتطلبات للبناء محليًا:
1. Windows 10/11.
2. Visual Studio 2022 مع Desktop development with C++.
3. CMake 3.24 أو أحدث.
4. اتصال إنترنت لأول Configure لتنزيل WebView2 SDK من NuGet.

من PowerShell داخل مجلد المشروع:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\build.ps1
```

الناتج: `build\Release\PrivaLayer.exe` مع مجلد `assets\ui` بجواره. يجب توزيع الـEXE ومجلد assets معًا.

## GitHub Actions artifact

نزّل Artifact باسم `PrivaLayer-x64` من تشغيل Workflow ناجح، ثم فك الضغط وشغّل `PrivaLayer.exe`. احتفظ بمجلد `assets` بجوار الملف التنفيذي؛ يحتوي على ملفات الواجهة المحلية.

## Shortcuts

الافتراضي:
- `Ctrl + Alt + Space`: إظهار/إخفاء PrivaLayer.
- `Ctrl + Alt + I`: تبديل Click-through.

يمكن تغيير الاختصارات من Preferences → Keyboard shortcuts. يجب أن تتضمن كل تركيبة مفتاح تعديل واحدًا على الأقل، وأن تكون التركيبتان مختلفتين وغير مستخدمتين من تطبيق آخر.

## Storage

- إعدادات النافذة والاختصارات: `%LOCALAPPDATA%\PrivaLayer\settings.ini`
- Notes وتفضيلات الواجهة: تخزين WebView2 المحلي في مجلد `%LOCALAPPDATA%\PrivaLayer\WebView2Data`.
- الصور المستوردة: `%USERPROFILE%\Pictures\PrivaLayer\`

## Browser profile

المتصفح المدمج يستخدم WebView2 profile منفصلًا عن Microsoft Edge. قد ترفض بعض المواقع تسجيل الدخول داخل WebView2؛ لا تنتقل جلسة Edge تلقائيًا إلى هذا المتصفح.

## Capture and screen sharing

التقاط الشاشة يبدأ فقط عندما تضغط زر Capture، ويستخدم واجهة Windows الرسمية لتحديد المنطقة يدويًا. بعد انتهاء القص ارجع إلى PrivaLayer واضغط Import. لا توجد لقطات تلقائية أو رفع للصور في الخلفية.

الشفافية وClick-through لا تضمنان استبعاد النافذة من كل برامج الاجتماعات أو التسجيل أو Remote Desktop. تختلف طرق الالتقاط من تطبيق لآخر؛ اختبر السلوك مع برنامج المشاركة الذي تستخدمه. لا يُقدّم التطبيق ضمان إخفاء مطلقًا.

## Notes about the AI assistant

AI Assistant مساحة لتجهيز Prompt وإدارة لقطة شاشة ثم فتح AI Studio في المتصفح المدمج. لا يوجد اتصال API مستقل أو إرسال تلقائي للنصوص والصور إلى Gemini؛ الإرسال يتم يدويًا من داخل الموقع.
