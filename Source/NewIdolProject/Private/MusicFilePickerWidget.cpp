#include "MusicFilePickerWidget.h"

#include "Components/Button.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Misc/Paths.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

UMusicFilePickerWidget::UMusicFilePickerWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    InitialDirectory = TEXT("C:/Users/user/Desktop/music");
    DialogTitle = TEXT("음악 파일 선택");
    SelectedFilePath.Reset();
}

void UMusicFilePickerWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (OpenMusicButton)
    {
        OpenMusicButton->OnClicked.AddDynamic(this, &UMusicFilePickerWidget::HandleOpenMusicClicked);
    }
}

void UMusicFilePickerWidget::SetInitialDirectory(const FString& InDirectory)
{
    InitialDirectory = InDirectory;
}

void UMusicFilePickerWidget::SetDialogTitle(const FString& InTitle)
{
    DialogTitle = InTitle;
}

void UMusicFilePickerWidget::HandleOpenMusicClicked()
{
    OpenMusicFileDialog();
}

void UMusicFilePickerWidget::OpenMusicFileDialog()
{
#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (!DesktopPlatform)
    {
        UE_LOG(LogTemp, Warning, TEXT("DesktopPlatform module is unavailable."));
        return;
    }

    FString StartDirectory = InitialDirectory;
    if (StartDirectory.IsEmpty())
    {
        StartDirectory = FPaths::ProjectDir();
    }

    StartDirectory = FPaths::ConvertRelativePathToFull(StartDirectory);

    TArray<FString> SelectedFiles;
    const FString TitleText = DialogTitle.IsEmpty() ? TEXT("Select Music File") : DialogTitle;
    const FString FilterText = TEXT("Audio Files (*.mp3;*.wav;*.flac)|*.mp3;*.wav;*.flac|All Files (*.*)|*.*");

    void* ParentWindowHandle = nullptr;
    if (FSlateApplication::IsInitialized())
    {
        const TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindBestParentWindowForDialogs(nullptr);
        if (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
        {
            ParentWindowHandle = ParentWindow->GetNativeWindow()->GetOSWindowHandle();
        }
    }

    const bool bOpened = DesktopPlatform->OpenFileDialog(
        ParentWindowHandle,
        TitleText,
        StartDirectory,
        TEXT(""),
        FilterText,
        EFileDialogFlags::None,
        SelectedFiles);

    if (bOpened && SelectedFiles.Num() > 0)
    {
        SelectedFilePath = SelectedFiles[0];
        OnMusicFileSelected.Broadcast(SelectedFilePath);
    }
#else
    UE_LOG(LogTemp, Warning, TEXT("Music file dialog is not supported on this platform."));
#endif
}

