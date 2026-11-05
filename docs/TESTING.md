# Проверка в виртуальной машине Windows 95

Нужна собственная установленная Windows 95 и снимок виртуальной машины.
В репозитории нет образов ОС или лицензий. Следующие проверки ещё не выполнены.

1. До включения автозапуска запустить EXE, открыть Preview. Проверить рисунок
   при 640×480 и 800×600, обычной и крупной системной гарнитуре.
2. Нажать Esc, Alt+F4, попробовать стандартное закрытие и сворачивание:
   экран должен остаться. Finish preview закрывает его не раньше чем через 1 с.
3. Проверить Enable → Disable → Enable и пути с пробелами. В реестре должны
   меняться только два значения `Welcome95`. Прочие записи должны сохраниться.
4. Перезагрузить Windows с включённым автозапуском. Отдельно зафиксировать,
   появляется ли фон раньше штатного окна входа. Это обязательная проверка,
   пока не подтверждённая прототипом.
5. На простой форме с сохранённым именем рамка сетевого диалога, подписи,
   исходное поле имени и Cancel должны исчезнуть. Имя должно отображаться
   справа на синем фоне, под ним настоящее поле пароля и зелёная стрелка.
   Проверить ввод, вставку, Backspace, Tab и Enter; мышью нажать OK.
   Нажатия Esc/Alt+F4 при фокусе в поле не должны отменять вход.
   Ввести ошибочный пароль: сообщение Windows должно остаться доступным поверх
   фона, а после его закрытия поле должно продолжать работать.
   Фон не должен исчезать, пока сеанс не стартовал.
   Успешно войти: после выполнения Run фон должен исчезнуть через 1–1,2 с
   на незагруженной системе; под нагрузкой таймер может сработать позднее.
6. Проверить отмену входа и вход без пароля. Закрытие здесь определяется
   стартом сеанса Windows, а не фактом проверки пароля.
7. Проверить исходную Windows 95 и требуемый OSR, русский/английский выпуск,
   Microsoft Windows Logon и используемый сетевой клиент по отдельности.
8. Проверить кнопку Trouble signing in?: полная исходная форма, имя и Cancel должны
   восстановиться и оставаться доступными. Проверить форму с пустым именем,
   доменом/дополнительными параметрами: такие формы должны сразу сохранять штатный вид.
9. Проверить отключение, повторную перезагрузку и отсутствие Welcome95.
   Во время входа открыть другое обычное/topmost окно и системный диалог:
   приветствие должно вернуться наверх в течение тика (100 мс без нагрузки).
   Фокус ввода при этом не должен сбрасываться. Ошибки штатного входа — исключение:
   они должны оставаться доступны выше фона. Ctrl+Alt+Del проверить отдельно,
   не считая его перекрытие гарантированным.
10. Сменить разрешение и проверить перемещение поля и зелёная стрелка. Отключить экран
    при незавершённом входе: стандартный диалог должен восстановиться.

## Тест восстановления на реальных Win32-окнах

```sh
sh tools/build.sh test-helper
```

Запустить `build/native_logon_test.exe` на тестовом рабочем столе Windows;
проверить код выхода (0 — успех) и `native-logon-test.log` в текущей папке.
Тест создаёт собственный диалог с вымышленными именем и паролем, проверяет
обрезку, расположение, сохранение родителя контролов, восстановление геометрии
и видимости, откат при слишком узкой области и сохранение формы с доменом/пустым
именем. Реестр, настоящие пароли и системный вход не используются.

Это тест модуля с локальными Win32-окнами; он не подтверждает межпроцессную
интеграцию, приём горячих клавиш до входа или порядок загрузки Windows 95.
32-битный EXE скомпилирован для запуска в тестовой Windows 95. В облаке исполнен
тот же исходный код, собранный для x64, под Wine64: прошли 36 проверок.
Ещё 9 проверок `tests/topmost_test.c` проверили приоритет над другим topmost-окном,
доступность принадлежащего диалогу сообщения и сохранение фокуса. Всего 45 прошедших
проверок. Команда повторения в подготовленной среде: `sh tools/test-wine.sh`.

## Восстановление

Disable восстанавливает исходную оболочку и удаляет только свои значения
автозапуска. Если оболочка не запускается, восстановить `[boot] shell` из файла
`W95xxxx.BAK` рядом с EXE через F8 → Command prompt only → EDIT SYSTEM.INI;
потом удалить только значения `Welcome95` из RunServices, Run и RunOnce.
Подробный порядок и место резервных данных: [BOOT_FLOW.md](BOOT_FLOW.md).
Не удалять целые разделы реестра, SYSTEM.INI или пользовательские профили.

## Проверено в облачной среде

- Компиляция i686 MinGW GCC 14 с предупреждениями как ошибками.
- Проверка PE32, версии подсистемы 4.0 и разрешённого набора импортов.
- Отсутствие внешнего CRT, .NET и TLS-инициализации в бинарном файле.
- 45 проверок реальных Win32-окон под Wine64/Xvfb, оба тестовых процесса завершились с кодом 0.

Межпроцессный ввод пароля, глобальные горячие клавиши до входа, изменение реестра
и загрузка Windows 95 требуют целевой системы; проверки Wine их не подтверждают.

## Пользователи и точность компоновки

Проверить список по ProfileList и [Password Lists] SYSTEM.INI, одинаковые имена
с разным регистром, отсутствие записей, более четырёх пользователей, переходы
между страницами. Содержимое PWL не читается. При выборе другого имени проверить
его передачу Windows и очистку ранее набранного пароля. Для каждого имени пройти
настоящий вход и убедиться, что Windows загрузила нужный профиль.

Сравнить 800×600 с референсом XP и проверить 640×480. Неподдерживаемый штатный
диалог не должен автоматически появляться поверх фона; возврат — только через
Trouble signing in?. Ошибка пароля должна оставаться доступной. Проверить Enter
при скрытой исходной кнопке OK и работу зелёной стрелки отдельно.

## Регрессия выбора карточки

Нажать другое имя, пока отображается Preparing sign-in…: выделение должно
измениться сразу, без требования уже подключённого диалога. После применения
имени штатным диалогом стрелка должна включиться. Повторить с пустым изначальным
именем, быстрым выбором нескольких карточек и переходом между страницами.
Сравнить метаданные welcome95-diagnostics.txt на целевой Windows 95, если
подключение не завершилось. Не записывать настоящие пароли в отчёты и видео.

## Скрытые дополнительные контролы сетевого входа Windows 95

Диагностика реальной системы содержит 14 контролов: активные Edit 21 (имя),
23 (пароль), скрытые отключённые Edit 25/29 и checkbox Button 26/30, а также
OK/Cancel и подписи. Оконный тест воспроизводит эти классы, IDs и стили,
проверяет успешное подключение, смену имени, очистку ввода и восстановление
скрытых отключённых контролов. Если дополнительный Edit становится видимым
или включённым, встраивание должно по-прежнему отклоняться.

На целевой Windows 95 повторить запуск: в новом диагностическом файле ожидается
`attached` вместо `unrecognized controls`; затем проверить реальный ввод,
выбор другого пользователя, ошибку пароля и успешный вход. Эти действия
пока не подтверждены облачными тестами.

## Состояния приветствия, выключение и шрифты

`sh tools/test-wine.sh` теперь выполняет 65 проверок: 36 штатного диалога,
22 состояния/приоритета/обработчика, 7 встроенных шрифтов. Проверяются скрытие
контролов загрузки, отдельное состояние welcome, переход по session-ready,
положительный ответ на запрос выключения, отмена и штатный выход обработчика.
Системное выключение в тестах не вызывается. GDI подтверждает регистрацию
четырёх TTF, нужные семейства/начертания и удаление временных файлов.

На целевой системе проверить один и несколько профилей, сохранение выбранного
имени, отсутствие поля ввода после старта сеанса, задержку не менее 1 с,
выключение из меню Windows и из Welcome95, отмену другим приложением. Для нового
скрытого обработчика необходимо повторно войти/перезагрузиться после обновления.
Проверить русские строки при CP1251, загрузку TTF 7.x на 95 и очистку TEMP.
Не требовать перекрытия финального системного экрана после завершения EXE.

## Подтверждение выбранных шрифтов XP

После замены шрифтов общее число проверок — 67. Две новые проверки читают SFNT
таблицу `name` реально выбранных GDI Arial Bold Italic/Tahoma и побайтово сравнивают
её со встроенными ресурсами. Это выявляет подмену другого шрифта с тем же
логическим именем. Файлы: Arial 3.00, Arial Bold Italic 2.90, Tahoma 3.15.

## VGA и регистр имён

Команды для проверенных конфигураций:

```sh
WELCOME95_TEST_SCREEN=640x480x16 sh tools/test-wine.sh
sh tools/test-wine.sh  # 800x600x24
```

Каждый запуск выполняет 82 проверки. Профильные тесты создают собственные
HKCU/INI-данные: смешанный регистр каталога, верхний регистр PWL-алиаса, источник
нативного имени Daniel, посторонний/отсутствующий кэш, управляющие символы
и допустимые не-ASCII символы. Тест рисования проверяет реальные GDI-пиксели
уменьшенного градиента на выход за RGB-диапазон исходника с допуском квантования.
Набор пиксельных проверок требует High/True Color (минимум 16 бит).

На Windows 95 повторить выбор и вход с Daniel, проверить фактические имена и
число профилей, кириллицу при ACP1251, область ввода и подписи в 640×480,
градиент в 16 и 24/32 битах. Ступеньки 16-битного цвета не означают порчу RGB.
В диагностике разрешены глубина/разрешение/ACP, но не имена и пути профилей.


## Окно выключения, фиксированные размеры и автовход

Текущий набор до изменения загрузки: 268 проверок в каждом режиме:

```sh
sh tools/test-wine.sh
WELCOME95_TEST_SCREEN=640x480x16 sh tools/test-wine.sh
WELCOME95_TEST_SCREEN=1024x768x24 sh tools/test-wine.sh
```

37 проверок штатного диалога, 23 состояний/приоритета, 9 шрифтов, 10 профилей,
3 RGB-рисования, 45 окна/диспетчера питания, 104 протокола DOS PIF с отказами и
37 фиксированной компоновки/автовхода. Все вызовы питания и запуска PIF в тестах
заменены инертными обработчиками. Окна настоящие; сеанс и пароли вымышленные.
Затемнение проверяется по пикселям снимка, восстановление — по окнам/состояниям.
Проверяется ровно одна штатная команда при одном аккаунте, сохранение ручного входа при неготовом
провайдере, запрет в preview/при нескольких аккаунтах и сохранение ручного ввода.

На Windows 95 дополнительно проверить:

1. Разрешения 640×480, 800×600, 1024×768 и выше: логотип, аватар, поле, стрелка и
   надпись welcome сохраняют размер. Привязки мыши/полей совпадают с рисунком.
2. Кнопка выключения: плавное затемнение, Tab/Enter, Esc, Alt+F4 и Cancel.
   Ввод и выбор профиля под окном недоступны; после отмены всё восстанавливается.
   Смена разрешения во время затемнения закрывает окно без оставшегося слоя.
3. Ожидание при доступном APM и возврат к полю входа после пробуждения.
   Недоступное ожидание должно быть отключено либо сообщать ошибку ОС.
4. Выключение/перезагрузка: штатные запросы сохранения доступны; отмена возвращает
   экран. Окно/затемнение не закрывают запросы другого приложения. Нет EWX_FORCE.
5. MS-DOS: галочка сама ничего не запускает; действует только Restart. Системный
   PIF и CONFIG/AUTOEXEC не меняются. Проверить реальный переход до входа и возврат.
6. Один аккаунт без пароля: штатный вход автоматически. Один с паролем: одна
   попытка, доступная ошибка, затем ручной ввод; никакого бесконечного повтора.
   Повторить при пустом исходном имени Windows и после неправильного пароля.
   При двух аккаунтах требуется выбор/ввод. Preview не пытается войти.
7. Русские подписи на русской ACP1251 Windows 95. Английский Wine не подтверждает
   растеризацию русского текста или межпроцессный вход при загрузке.


Компактный режим 640×480: сравнить список с двумя аккаунтами, первичное отсутствие
поля, раскрытие первого и второго имени, прокрутку к пятому/последнему профилю и
обратно. У второго раскрытого имени стрелка/поле не должны пересекать нижнюю
панель. При смене разрешения выбранная карточка остаётся открытой. На загрузке
welcome находится над карточкой. Проверить мышь, ползунок и клавиши прокрутки
нативного контрола; его системное оформление на Windows 95 отличается от Luna.
Провайдер с первоначально отключённой OK остаётся ручным даже после её включения:
уже набранный текст не должен очищаться поздней автопопыткой.


## Мерцание, посредник оболочки и logoff

Набор до исправления пути загрузчика: 332 проверки на каждом разрешении. Дополнительно 31 проверка
установки используют приватные HKCU/INI/BAK-данные. 33 проверки сеанса используют
настоящие отдельные процессы: синхронный запуск, оболочка-метка и искусственный
провайдер. Это не настоящий системный вход или logoff. 100 стабильных вызовов
приоритета не дают WM_WINDOWPOSCHANGING; восстановление после конкурента не меняет
фокус. В 1.0 метка дочерней оболочки появляется за ещё видимым приветствием
после задержки 1 с; обычный /startup остаётся заблокирован до удаления окна.

Повторить три команды из предыдущего раздела. На Windows 95:

1. Сохранить снимок VM. Disable старой версии → закрыть настройки → заменить EXE
   → Enable новой версии. Убедиться, что появился читаемый W95xxxx.BAK с прежним
   shell и что прочие INI/Run-значения сохранены. Перезагрузиться.
2. На обычном и сетевом входе проверить поле/стрелку/карточки без моргания.
   Повторить с внешним topmost-окном, ошибкой пароля и переключением выбранного имени.
3. Проверить, что после подтверждённого входа и секунды Welcome исходная
   оболочка запускается за приветствием, затем экран убирается без анимации.
   Обычный /startup ждёт закрытия экрана. Отдельно проверить Explorer95,
   стандартный Explorer, оболочку с параметрами и без собственного окна.
   RunServices и программы, запускаемые самой оболочкой, не приостанавливаются.
4. Выполнить logoff, а не перезагрузку, войти снова; повторить несколько раз.
   Старый сигнал не должен сразу закрывать новый экран. Проверить один/несколько
   аккаунтов, пароль/пустой пароль, кириллицу и разные разрешения.
5. Отменить завершение другим приложением, в том числе пока экран входа открыт:
   сохранить экран, настоящее поле и доступность Save/Cancel/Esc системного запроса.
6. Проверить штатное выключение и освобождение временных шрифтов. При уведомлении
   без причины контроллер остаётся скрытым до следующего входа или остановки ОС.
7. Disable новой версии должен вернуть исходную оболочку с параметрами. После
   перезагрузки Welcome95 отсутствует; резервные файлы остаются. Восстановление
   из DOS проверить по BOOT_FLOW.md. Не затирать весь SYSTEM.INI файлом BAK.


## Не-ASCII путь, SHELL без параметров и аварийный возврат

Текущий набор: 346 проверок в 800×600×24, 640×480×16 и 1024×768×24.
40 проверок установки включают миграцию прежних Run-записей после ручного возврата
shell, откат gate, ASCII-путь без параметров и повторное Enable. 38 проверок сеанса
запускают через Win32 основной EXE из не-ASCII ANSI-папки с пробелами; загрузчик
обслуживает Windows-сообщения, пока посредник ждёт удаления welcome.

Скрытое сочетание тестируется через сообщение контроллеру: стандартные рамка,
родитель поля и диалог восстанавливаются, фон скрывается, ready/closed не подаются.
Отдельно проверено доступное сообщение провайдера без владельца (обычная ошибка
Windows не должна оказаться под фоном). Пароль не читается и не отправляется.

На русской Windows 95 повторить исходную папку «Рабочий стол». В SYSTEM.INI должно
быть только ASCII-имя W95BOOT.EXE, без `/shell` и без пути рабочего стола. После
входа должен появиться welcome, затем оболочка; повторить logoff, неправильный
пароль, окно выключения/отмену и физическое Ctrl+Alt+Shift+L. Если проблема остаётся,
сравнить метаданные W95START.LOG рядом с ASCII-загрузчиком и native diagnostics;
не записывать настоящие пароли в отчёт. NT/Wine не доказывает Win95 OEM/ANSI загрузку.

`sh tools/build.sh all test-helper` собирает встроенный x86-загрузчик.
`python3 tests/check_pe.py build/w95boot.exe --bootstrap` проверяет его подсистему
4.0, три базовые DLL и отсутствие CRT; основной EXE проверяется обычной командой.


## Native Edit visibility and owner-drawn button damage

Current suite: 363 checks per resolution (640x480x16, 800x600x24, 1024x768x24).
The native fixture changes only the child visibility/rectangle or region while
leaving the outer dialog untouched; all three failed against the previous cache.
Repair preserves synthetic input and OS-disabled state. A hundred stable checks
must not change styles or window positions.

The external-provider fixture verifies WindowFromPoint reaches the actual Edit,
that its own thread receives keyboard focus after a border click, and that a
canvas repaint queues a real WM_PAINT on that Edit. It never reads real passwords
or treats the inert provider's OK as authentication.

Power tests use the production WS_CLIPCHILDREN style and inspect real red-icon
and blue-background pixels, including black damage followed by a parent repaint
and shade cancellation. Offscreen GDI tests cover a nonzero DRAWITEMSTRUCT origin,
full/focus-only repaint and preservation of the caller's drawing state.

On real Win95, repeat logoff, click the white field/border and type: caret and
stars must appear. Check wrong-password/retry, cancel the power dialog repeatedly,
cover/uncover the window and change resolution. Check the real power button icon
and text, not only clickability. If rendering still differs, compare the geometry,
clipping and hit-class metadata in welcome95-diagnostics.txt. Video/Wine do not
establish the exact Win95 provider or display-driver message sequence.


## Win95 missing-export regression

Current suite: 365 checks per resolution, including 45 native-control checks.
The region probe uses GetWindowRgn + GDI32.GetRgnBox and must preserve an original
nonrectangular provider region after repeated idle checks and changed layout.
The PE checker explicitly rejects GetWindowRgnBox; it was also run against the
tracked c77ab64 EXE and rejected its exact USER32 import. Both rebuilt production
images pass. Wine does not establish target export availability: repeat actual
Win95 launch, native field typing and power repaint after updating the EXE.


## Logged Win95 client offset and actual keyboard input

Current suite: 370 checks per resolution (48 native, 44 session/lifecycle).
Both native fixtures retain a (3,22) client origin via WM_NCCALCSIZE even if frame
styles are stripped. This reproduces the logged region (3,22)-(170,44) below a
22px outer window. Compiling the fixture against 8d66912 reproduces failed field
hit tests and client positioning; the new crop keeps original window dimensions
and styles and exposes only the measured Edit client. Check both reported forms
(Edit 23 in 14 controls; Edit 31 in 8 controls) and original-region restoration.

The external-provider fixture uses the same legacy origin, real SetCursorPos /
mouse_event / keybd_event input and metadata-only EM_GETMODIFY checks. Mask pixels
must be visible inside the field. A queued activation after the provider hides its
Edit is processed before the following normal priority pass. Tests do not read
real passwords or dispatch real authentication/power.

On Win95, click the actual field and type: caret and stars must appear; perform
correct and incorrect login on both provider forms. Then repeat logoff, hidden
rescue and power cancel. Diagnostics now record parent outer/client sizes too;
the exposed client rectangle must fit inside both the parent client and window.


## Larger native font and full Russian translation

Earlier localization suite: 387 checks per resolution, including 50 native, 45 session and
14 Russian UI checks. Native tests begin with an 11px fixture font, select the
larger provider default without changing field geometry, and verify original-font
restoration. The provider measures its selected font locally; no foreign GDI
handle is interpreted as a local object. Real input and mask-pixel checks remain.

ui_test64 uses WELCOME95_TEST_RUSSIAN only at build time. It inspects actual
Russian settings/login/power captions and measures their CP1251 glyph widths in
single-line buttons and wrapping text areas at all three resolutions. Wide APIs
are confined to this Wine test; production remains ANSI Win95. Its language
selection uses ACP1251, with the English fallback for other code pages.

Repeat on Win95: larger visible masks, no field resizing, standard-dialog original
font after Ctrl+Alt+Shift+L, complete Russian controls/errors and labels without
clipping at 640x480. Follow the normal Disable/replace/Enable/reboot update flow
so the newly embedded bootstrap is deployed too.


## Startup screen, quiet single-account attempt and compact settings

Current suite: **407 checks per resolution**, passed under Wine64/Xvfb at
640×480×16, 800×600×24 and 1024×768×24. Includes 54 native-control, 45 layout,
51 session/cross-process and 16 Russian UI checks. Production EXE and bootstrap
are rebuilt and independently audited for PE4.0/i386, reviewed imports and no CRT.

The quiet probe must preserve native geometry/styles/Cancel/focus/font and reject
EM_GETMODIFY input without reading its contents. Startup hides chooser controls
and does not signal readiness or closure after posting native OK. Only the OS
ready event enters Welcome. Disabled OK, notices, a replacement form and bounded
timeout restore manual login without retrying or clearing subsequent typed input.
The separate-provider fixture also checks normal reattachment and shutdown-cancel
while probing. Settings tests check all child bounds and CP1251 text fit inside
the actual 410×234 outer window; the startup preview is inert.

Target Windows 95 checks:

1. One passwordless account: startup → lone greeting → shell, without chooser flash.
2. One protected account: native error remains accessible, then visible password
   field; correct/wrong manual passwords behave as before, no automatic retry.
3. Slow/replaced/disabled provider: manual input appears after fallback and works.
4. Repeat logoff/relogon, power cancellation and the hidden native-dialog rescue.
5. Compare the smaller Russian greeting and compact settings at VGA and 1024×768.
   English greeting stays 36pt; Russian is 32pt.

After the final 40px startup-logo move, the 7 rendering and 16 Russian UI checks
passed again at VGA; the refreshed startup capture was inspected.

The new docs/previews/starting-en.png and settings-compact-en.png are real Wine
captures with ACP1252 at VGA (no window-manager decorations), not Win95/Russian
screenshots. Russian control text and geometry are measured in the test build.


## XP status-position correction

Compare the actual inert /preview-starting and /preview-shutdown captures at
640×480 against the user's XP status reference. The shared composition has logo
bitmap origin (355,168), right edge x=472 for the status line and text-area top y=274.
The first non-background logo pixel is offset (3,5) inside the supplied 137×86
bitmap. At 1024×768 the origin must move by (+192,+144), without scaling. Compare
startup and shutdown with their distinct text; switching modes must not move the
artwork. Text is right-aligned to the original 384px panel
with 40px right padding (logo uses 20px); different messages keep the same right
edge. The footer divider is 2px, as in the original resource UIFILE.

Use Arial 14pt for status text, matching original UIFILE rcstr(1)/rcint(45).
The rebuilt main/boot PEs pass the compatibility audit. The full 407-check Wine
suite passed at 640×480×24 for the placement correction; rendering 7/UI 16 checks
passed again after restoring the original status-font size and right alignment. Real Win95 GDI
rasterization remains a target check.

## Managed LOGON installation and manual single-account loading

Current suite: 426 checks per resolution, passed under Wine64/Xvfb at
640×480×16, 800×600×24 and 1024×768×24. Includes 57 startup, 46 layout, 10 font and 51
session checks. Production main/boot PE audits pass (174/50 reviewed imports);
new CopyFileA/ReadFile imports are baseline Win95 file APIs.

The layout fixture now sets a private OS-ready event after manual fallback with
exactly one profile: the named loading card must appear, the closed event must
remain unset. Existing quiet-start checks still require the lone greeting.
The screenshot docs/previews/manual-single-640-en.png is an inert actual Wine
window following this same transition with one synthetic Daniel account.

Startup filesystem tests copy a non-ASCII source into a private WINDOWS/LOGON
with no boot changes, validate ownership, prevent repeated self-copy, preserve
a locked or foreign EXE, retain a unique previous-EXE backup and reject changed
markers. Private HKCU startup tests migrate an active legacy install, reject
foreign entries, simulate failure of the new shell write after legacy Disable,
and verify restoration of exact owner/shell/startup metadata. Success must
preserve the original shell and create new recovery files/journal inside LOGON.
A separate managed font process must load/clean all resources within LOGON.
DOS PIF tests use a private Windows/LOGON directory and inert launch callbacks.
No real startup, authentication, DOS or power operation is performed by tests.

On the Windows 95 VM:

1. Start the new download from Desktop; verify settings now run from
   %WINDIR%\LOGON\WELCOME95.EXE. Press Enable, reboot and inspect SYSTEM.INI's
   ASCII bootstrap route plus the original-shell recovery BAK inside LOGON.
2. Install over an enabled old Desktop copy, then Disable/re-enable from LOGON.
   Original shell/parameters must survive. Retain old backups/cached loaders.
3. Update after Disable/closing settings; expect a unique E95xxxx.BAK in LOGON.
   A busy or foreign destination must fail before changing startup.
4. One protected account: type the password, verify greeting plus selected name,
   avatar/loading text, then the usual delayed shell handoff. One passwordless
   account: quiet startup then lone greeting, without manual input.
5. Repeat wrong-password recovery, hidden native rescue, logoff/relogon and power
   cancellation. Inspect both logs and font/boot/DOS files under LOGON.
6. Compare Russian UI/GDI at VGA and larger modes. Actual Win95 rendering and
   native authentication remain target checks, beyond Wine fixtures.

## Access-denied Enable regression and setup journal

Current suite: 433 checks per resolution, including 64 startup checks. The startup
test now includes the production module behind narrow Win32 API-contract
adapters while using real private HKCU keys/files. Enforce RegFlushKey's
KEY_QUERY_VALUE requirement; model ACCESS_DENIED on an existing destination.
Inject one real flush denial separately: setup must fail before installing
startup entries or activating the gate. The preceding dff7f7f source reproduces
both contract failures; the corrected module passes.

Verify that a backup-name collision preserves the existing file and allocates
a different complete BAK, that initial setup creates W95START.LOG before any
Owner value exists, and that a genuine denial reports
install-reg-backup-Valid-flush/code 5 with no paths or shell contents in the log.
The ordinary Wine registry implementation does not enforce the flush-rights
contract, so an unadapted passing Wine run would miss this defect.

Target Win95: update through the normal Disable/close/new EXE/Enable flow.
If Enable still fails, retain the numeric error, exact stage from the dialog
and %WINDIR%\LOGON\W95START.LOG. Do not discard original shell backups or
manually overwrite foreign registry/file values. No target Enable has been
executed in the cloud; the original screenshot identifies no exact failing API.

API contract source: https://github.com/MicrosoftDocs/sdk-api/blob/docs/sdk-api-src/content/winreg/nf-winreg-regflushkey.md

## Logoff selection and native Control Panel entry

Current suite: 465 checks per resolution: 58 session, 25 Control Panel and 64
startup checks among the existing suites. Standard modes: 640×480×16, 800×600×24
and 1024×768×24. Production main/boot/CPL audits remain i386/PE4.0/no CRT/TLS.
The CPL exports only CPlApplet; its test binding export exists only in the
64-bit fixture.

Session tests load exactly one profile from private HKCU/INI, retain cold
startup and cancellation behavior, and assert closed cards after logoff and
reasonless session recovery. The separate native provider counts commands:
no OK before selection, no automatic OK after selection, exactly one OK after
an explicit submit. The existing real mouse/keyboard/mask-pixel and rescue
checks still run. Real OS authentication/logoff are not performed.

Applet tests use private Cpls/Entry records and load the actual 64-bit CPL.
They validate metadata/icon/export and capture a quoted managed-EXE launch
without creating the real process. They exercise first-install failure cleanup,
loaded-module reuse/update, registration rollback, settings access after Disable,
foreign registration and altered ownership/hash. No host Control Panel values
or actual boot configuration are changed.

Target VM checks:

1. Cold boot, one passwordless account: original quiet auto-login remains.
2. Logoff: list waits for selection even with one account; field/arrow appear
   only after selection, no blank attempt is posted automatically. Sign in
   explicitly and repeat. Verify logoff cancellation and VGA/larger layouts.
3. Enable: open Control Panel and find Welcome95; double-click opens the managed
   EXE settings. Reopen/refresh an already open Control Panel if needed.
4. Disable from that window; the icon should still open Settings for reenabling.
5. Update with Control Panel open: the new registration must coexist safely
   with the old loaded module. Retain original shell/backups and reboot normally.
Native Control Panel enumeration and real Win95 authentication need target
confirmation beyond these Wine fixtures.


## Logoff canvas lifetime and first full repaint

The session fixture now waits after committed logoff with no provider: no canvas,
children or session fonts may be allocated, and no close acknowledgement may be
published. A separate inert provider then triggers the new canvas. Cancelled
logoff, reasonless-end/ready fallback, hidden rescue and authenticated one-second
handoff retain their existing checks.

A contrasting full-screen background and consumed construction/show update
regions model the missing-background case. Before pumping further input, verify
actual GDI pixels in the top band, middle and footer, with eight-level tolerance
for 16-bit channel quantization. Construction must remain hidden; first visible
painting must have the published canvas HWND. Count every production SetEvent on
CanvasClosed so a transient set/reset during retirement cannot escape detection.

Target check: on Windows 95/86Box, repeat logoff without touching the mouse or
keyboard; the complete account-choice canvas must appear without a click and
without an isolated power button over wallpaper. Then select the sole account,
enter via the real provider, cancel logoff once and repeat. The cloud test models
the update-region contract; it does not establish the exact Win95 teardown cause.


Current combined suite: 486 checks per resolution, including 64 session, 40 CPL
and 64 startup checks, passed at 640x480x16, 800x600x24 and 1024x768x24 under
Wine64/Xvfb. The CPL fixture discovers its DLL through a private CONTROL.INI,
loads static icon/name/description with CPL_INQUIRE alone, checks Russian/English
resources and covers denied writes, explicit flush failures, zero-success flush,
readback mismatch, registry-only upgrade, loaded-version rollback and a foreign
INI change between apply and rollback. All launches are inert callbacks.
The native PE audit additionally validates the x86 icon and both STRINGTABLEs.


## Missing MMCPL key and panel-ini-owner / 170

The user reported panel-ini-owner/code 170 and confirmed there was no Welcome95
entry in CONTROL.INI. The CPL fixture now includes the production backend behind
a narrow profile-query adapter that normalizes a nonprinting missing-key default
to empty. With the preceding 1fa29fe backend this reproduces code 170 before
installation; the fixed backend enumerates keys and permits the absent entry.
The exact profile-API return in the target VM has not been captured.

Check absent CONTROL.INI, a section with only unrelated entries, a present-empty
mixed-case key, an actual foreign value and a truncated key inventory. The same
adapter remains active through the existing real DLL/private registry/INI
installation, update, rollback and unregistration cases. No production path may
request a nonprinting default marker. Existing empty/foreign entries stay protected.


Current validation after this fix: 493 checks each at 800x600x24 and 640x480x16
under Wine64/Xvfb, including 47 CPL, 64 startup and 64 session checks. Main,
bootstrap and CPL PE/resource audits pass with unchanged import counts. A
separate private probe compiles the preceding backend and the fixed backend
against the same normalized-default adapter: 170/panel-ini-owner versus success.
Retry Enable and native Control Panel discovery in the target Win95 VM.


## White button flashes, cancellation and early first frame

Current suite: 506 checks each at 800x600x24, 640x480x16 and 1024x768x24 under
Wine64/Xvfb, including 59 power and 66 session checks. The power fixture models
old USER clearing a BUTTON to white before owner draw; it first verifies those
white pixels, then verifies our power/submit/action controls bypass that path
through erase and repaint. Full parent damage still repairs pixels, while damage
away from the footer does not repaint its button. Native BM_CLICK still opens
the inert choice and submits the preview action.

Delay each shade paint by 100ms: the fade must skip obsolete phases instead of
replaying sixteen timer steps, then become paint-idle. During Cancel's WM_ENABLE,
verify the shade is still present, the modal guard remains active and a reentrant
welcome timer cannot reorder the canvas over it. Existing dispatch/disabled-state
and real cross-process password tests remain enabled and inert.

An initial-cycle fixture releases fonts and inspects the actual first-frame
pixels before profiles_load and bundled_fonts_load. A ready timer injected during
resource preparation cannot finish welcome or submit; quiet-start, logoff-choice
and post-destruction shell-gate checks still pass. Production main/boot/CPL audits
pass (175/51/15 imports), including existing static CPL resources; helpers match
the embedded bytes. CallWindowProcA is the classic Win95 subclassing addition.

Target Windows 95/86Box: open/cancel power repeatedly with and without an open
account; inspect the power button/arrow/action buttons for white/blank frames.
Repeat mouse, keyboard, shade cancellation and genuine native input. Cold boot
and compare the first startup frame against appearance of the stock dialog.
Cloud tests do not execute Win95's old USER or establish its service scheduling
or hardware presentation speed.


## Hidden-only rescue, queued power opening and early covers

Current suite: 519 checks each at 800x600x24, 640x480x16 and 1024x768x24 under
Wine64/Xvfb, including 64 power and 74 session checks. The removed native-rescue
button (former ID 106) must be absent in Russian UI and repeated-login states;
the existing hidden hotkey still restores the external native provider without
submitting credentials or announcing authentication.

A native power BM_CLICK must return with only a pending posted request, before
snapshot/dialog work begins. Inject a wait pointer into our welcome/button/shade/
choice clients and verify WM_SETCURSOR restores the arrow. Keep all previous
white-erase, slow-paint, cancellation and actual native-input regressions.

A separate fake frontend delays initialization for 600ms: the actual bootstrap
must show and paint its early cover first, then remove it when that frontend
paints and announces ControllerReady. Poll for the first actual painted pixels;
IsWindowVisible alone can precede completion of WM_PAINT. Test failing-frontend
cleanup, duplicate-controller handling and unchanged SessionReady/CanvasClosed.
After committed logoff, check the same existing controller HWND is visible with
no child credential controls, and is hidden after the replacement canvas paints.

PE/resource audits: main 176 imports/5 system DLLs; bootstrap 70 imports/4 DLLs
including classic GDI32; CPL 15 imports/3 DLLs. Exact helper bytes are embedded in
the tracked x86 EXE. Actual Win95 service scheduling, pointer flicker and physical
first-frame timing still require the user's 86Box VM.

The supplied Internat screenshot reports an invalid page fault in <unknown> at
0000:00000009. It does not identify an Explorer95 or Welcome95 module. No crash
fix or suppression is claimed; compare stock logon with the same shell before
attributing the fault. No other repository or Internat startup entry was changed.


## 1.0: оболочка за приветствием и ресурсы выпуска

528 проверок в каждом режиме Wine64/Xvfb: 640×480×16, 800×600×24 и
1024×768×24. Из них 83 сеансовых проверки, 64 установки, 64 выключения и
47 элемента Панели управления. Новая проверка запускает инертную оболочку
в отдельном процессе: она пишет метку, пока приветствие ещё видно и обычный
/startup заблокирован. Удаление окна и CanvasClosed следуют за запуском.

Отдельно проверены скрытое/видимое окно оболочки, чужой PID, ограниченное
ожидание работающего процесса без окна и отсутствующего подтверждения запуска,
а также сброс разрешения оболочки при logoff. До OS-ready и секунды Welcome
разрешение не выдаётся. Сценарии настоящего пароля, logoff и запуска оболочки
Windows 95 по-прежнему требуют целевой VM.

PE-аудит основного EXE проверяет FILEVERSION/PRODUCTVERSION 1.0.0.0 и все восемь
кадров ICO. CPL-аудит проверяет те же кадры и статические имена/описания на двух
языках. Main/boot/CPL: 177/70/15 разрешённых импортов, Windows 4.0, без CRT/TLS.
Иконки сравниваются с ICO побайтово; выбранные 4/8-битные DIB-кадры совпадают
с исходным пользовательским welcm95.ico, без PNG/ARGB для старой Windows.


## 2026-11-05: ожидание без смены фона

533 проверки на каждом из трёх режимов Wine64/Xvfb (640×480×16, 800×600×24,
1024×768×24), включая 88 сеансовых. Снимок настоящего окна ожидания сохраняется
до его замены, затем сравнивается с первым кадром «Запуск Windows...», исключая
только область изменяющегося текста. Фон, свечение, границы полос и логотип
должны совпадать. Оба кадра копируются в одинаковый формат дисплея, чтобы
округление RGB565 при сохранении одного из них не считалось изменением фона.

Принудительно скрыть и изменить размер собственного окна ожидания: оно должно
восстановиться до появления нового провайдера. Сто тиков стабильного ожидания
не должны давать WM_WINDOWPOSCHANGING или WM_PAINT. Отложенный запрос старой
фазы не должен показать ожидание снова после передачи новому экрану. Эти новые
проверки сначала выявили три дефекта в прежней реализации, затем прошли после
исправления. Импорты main/boot/CPL не изменились (177/70/15), PE-аудит пройден.

В Win95/86Box повторить переход из видео, в том числе при сбросе разрешения
профиля. Облачный тест подтверждает логику и GDI-пиксели собственных окон;
физический порядок кадров и конкретная операция скрытия Windows требуют VM.


## 2026-11-05: проверка промежутка до обработки очереди

Повторное видео Win95/86Box показало оставшийся провал примерно на 0,27 с,
несмотря на совпадающее оформление. Поэтому проверки теперь контролируют
видимость сразу после ShowWindow(SW_HIDE), без ожидания обработки сообщений.
Первый кадр нового окна проверяется внутри SWP_SHOWWINDOW: тест подставляет
бирюзовый фон перед обработчиком и проверяет, что он уже заменён настоящими
пикселями к возврату обработчика. Оба сценария сначала упали на ee38844.

538 проверок прошли в каждом режиме Wine64/Xvfb: 640×480×16, 800×600×24,
1024×768×24, в том числе 93 сеансовых. Проверены немедленные hide/demotion,
сохранение ожидания при общем WM_CLOSE и его настоящее уничтожение через Stop.
Сохранены существующие проверки горячей клавиши, ввода провайдера, отсутствия
ложной готовности, одинаковых кадров и простоя без лишней перерисовки.
Импорты main/boot/CPL остались 177/70/15; PE/ресурсные проверки пройдены.

Для целевого прогона использовать новую сборку после ee38844. При повторении
провала сопоставить видео с canvas-create-start, canvas-show-painted,
canvas-presented и cover-*-held в W95START.LOG. По одному видео нельзя установить,
скрыла ли Windows окно, изменила порядок или показала ещё не нарисованный кадр.
Факт прохождения Wine не означает, что физический переход Win95 уже исправлен.


## 2026-11-05: готовый первый кадр до создания окна

Следующее видео показало остаточные 11 бирюзовых кадров (~0,18 с). Целевой журнал
сохраняет PID контроллера и показывает 62–68 мс между созданием и первым показом;
cover-*-held в присланном фрагменте отсутствуют. Это наблюдения, а не доказанная
точная причина провала. Первый кадр теперь готовится до CreateWindowEx, полный
набор шрифтов — после первого показа. В видимом SWP_SHOWWINDOW остаётся копирование
готового изображения. Для обычной отрисовки используется та же композиция.

541 проверка прошла на каждом из режимов Wine64/Xvfb: 640×480×16, 800×600×24,
1024×768×24; из них 96 сеансовых. Новая проверка распределения bitmap сначала
упала на 7aac53c. Она контролирует подготовку за видимым старым экраном, отсутствие
выделения нового кадра внутри видимого show и отложенные шрифты интерфейса.
Сохранены сравнение реальных кадров и синхронный показ с искусственной порчей
фона. Дополнительно проверены очистка DC/bitmap после показа и принудительный
отказ подготовки с обычной отрисовкой без преждевременного CanvasClosed.

PE/ресурсные проверки пройдены; импорты main/boot/CPL остались 177/70/15.
На целевой Windows 95 сопоставить видео с canvas-frame-start/ready, затем
canvas-create-start, canvas-show-start, canvas-show-painted, canvas-presented.
Прохождение облачного набора не подтверждает устранение физической паузы Win95.
