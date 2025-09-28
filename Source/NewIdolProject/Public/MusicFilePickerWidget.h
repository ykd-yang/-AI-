#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MusicFilePickerWidget.generated.h"

class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMusicFileSelected, const FString&, FilePath);

UCLASS()
class NEWIDOLPROJECT_API UMusicFilePickerWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UMusicFilePickerWidget(const FObjectInitializer& ObjectInitializer);

    virtual void NativeConstruct() override;

    UPROPERTY(BlueprintAssignable, Category="Music")
    FOnMusicFileSelected OnMusicFileSelected;

    UFUNCTION(BlueprintCallable, Category="Music")
    void SetInitialDirectory(const FString& InDirectory);

    UFUNCTION(BlueprintCallable, Category="Music")
    void SetDialogTitle(const FString& InTitle);

    UFUNCTION(BlueprintPure, Category="Music")
    FString GetSelectedFilePath() const { return SelectedFilePath; }

protected:
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="Music")
    TObjectPtr<UButton> OpenMusicButton;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Music")
    FString InitialDirectory;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Music")
    FString DialogTitle;

    UPROPERTY(BlueprintReadOnly, Category="Music")
    FString SelectedFilePath;

    UFUNCTION()
    void HandleOpenMusicClicked();

private:
    void OpenMusicFileDialog();
};

