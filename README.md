# PrivaLayer — Personal Workspace

واجهة Windows مبنية بـ C++20 وWin32 API وMicrosoft WebView2، تجمع الملاحظات والبحث وأدوات تجهيز السياق للذكاء الاصطناعي في مساحة واحدة.

## Features

- واجهة Overview وNotes وBrowser وAI Assistant وWorkspace وPreferences.
- Notes متعددة مع بحث وإنشاء وحذف وحفظ تلقائي محلي.
- متصفح WebView2 متعدد التبويبات: إنشاء وإغلاق وتبديل Tabs، شريط عنوان/بحث، رجوع، تقدم، تحديث، وصفحة بداية Google. يمكن فتح Google AI Studio في تبويب مستقل.
- التقاط كامل للشاشة عند الضغط على Capture أو الاختصار الافتراضي `Ctrl + Alt + P`: يحفظ PNG في `Pictures\PrivaLayer` ويضع الصورة في Clipboard لتلصقها مباشرة بـ `Ctrl + V`.
- Preferences: ثلاث Themes، شفافية النافذة، إبقاء النافذة بالأعلى، Click-through، وطي/توسيع Sidebar.
- اختصارات عامة قابلة للتخصيص لإظهار/إخفاء النافذة، تبديل التفاعل، والتقاط الشاشة.
- System Tray menu لإظهار التطبيق أو فتح Settings أو الخروج.

## Build

يتوفر GitHub Actions workflow للبناء تلقائيًا على Windows x64 عند Push أو Pull Request أو يدويًا من تبويب Actions.

المتطلبات للبناء محليًا:
1. Windows 10/11.
2. Visual Studio 2022 مع Desktop development with C++.
3. CMake 3.24 أو أحدث.
4. Node.js لفحص صياغة ملفات JavaScript، واتصال إنترنت لأول Configure لتنزيل WebView2 SDK من NuGet.

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
- `Ctrl + Alt + P`: التقاط كامل للشاشة، حفظ PNG ونسخ الصورة إلى Clipboard.

يمكن تغيير الاختصارات من Preferences → Keyboard shortcuts. يجب أن تتضمن كل تركيبة مفتاح تعديل واحدًا على الأقل، وأن تكون التركيبات الثلاث مختلفة وغير مستخدمة من تطبيق آخر.

## Storage

- إعدادات النافذة والاختصارات: `%LOCALAPPDATA%\PrivaLayer\settings.ini`
- Notes وتفضيلات الواجهة وبيانات المتصفح: `%LOCALAPPDATA%\PrivaLayer\WebView2Data`
- الصور الملتقطة: `%USERPROFILE%\Pictures\PrivaLayer\`

## Browser profile

المتصفح المدمج يستخدم WebView2 profile منفصلًا عن Microsoft Edge. قد ترفض بعض المواقع تسجيل الدخول داخل WebView2؛ لا تنتقل جلسة Edge تلقائيًا إلى هذا المتصفح.

## Capture and screen sharing

التقاط الشاشة يتم فقط بطلب مباشر من المستخدم من زر Capture أو اختصار الالتقاط. يلتقط سطح المكتب الافتراضي كاملًا، ويحفظ الصورة محليًا ويضعها في Clipboard. لا توجد لقطات تلقائية أو رفع للصور في الخلفية.

الشفافية وClick-through لا تضمنان استبعاد النافذة من كل برامج الاجتماعات أو التسجيل أو Remote Desktop. تختلف طرق الالتقاط من تطبيق لآخر؛ اختبر السلوك مع برنامج المشاركة الذي تستخدمه. لا يُقدّم التطبيق ضمان إخفاء مطلقًا.

## AI assistant

AI Assistant مساحة لتجهيز Prompt وفتح Google AI Studio في المتصفح المدمج. لا يوجد اتصال API مستقل أو إرسال تلقائي للنصوص والصور إلى Gemini؛ الإرسال يتم يدويًا من داخل الموقع.
