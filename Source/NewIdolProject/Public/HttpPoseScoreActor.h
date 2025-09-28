#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HttpPoseScoreActor.generated.h"

class USceneComponent;
class UTextRenderComponent;

/**
 * Actor that renders incoming pose score information using a text render component.
 * Mirrors the behaviour described in UnrealHttpServer_Blueprint.md for the BP_Score blueprint.
 */
UCLASS()
class NEWIDOLPROJECT_API AHttpPoseScoreActor : public AActor
{
	GENERATED_BODY()

public:
	AHttpPoseScoreActor();

	/** Updates the display with the latest message and confidence value. */
	UFUNCTION(BlueprintCallable, Category = "HTTP")
	void UpdatePoseData(const FString& Message, double Confidence);

	/** Sets a temporary debug string on the text component. */
	UFUNCTION(BlueprintCallable, Category = "HTTP")
	void SetDebugText(const FString& InText);

	/** Returns the last message received. */
	UFUNCTION(BlueprintPure, Category = "HTTP")
	FString GetLastMessage() const { return LastMessage; }

	/** Returns the last confidence value received. */
	UFUNCTION(BlueprintPure, Category = "HTTP")
	double GetLastConfidence() const { return LastConfidence; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Root scene component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* Root;

	/** Text component used to display the current status. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UTextRenderComponent* TextComponent;

	/** Text colour used for the render component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FLinearColor TextColor = FLinearColor::White;

	/** Size of the text that is rendered in the world. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	float TextSize = 48.f;

private:
	FString LastMessage = TEXT("Awaiting pose data...");
	double LastConfidence = 0.0;

	void RefreshTextRender();
};
