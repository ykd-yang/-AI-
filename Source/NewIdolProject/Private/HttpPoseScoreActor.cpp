#include "HttpPoseScoreActor.h"

#include "Components/TextRenderComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"

AHttpPoseScoreActor::AHttpPoseScoreActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	TextComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TextRender"));
	TextComponent->SetupAttachment(Root);
	TextComponent->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	TextComponent->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
	TextComponent->SetText(FText::FromString(LastMessage));
	TextComponent->SetTextRenderColor(TextColor.ToFColor(true));
	TextComponent->SetWorldSize(TextSize);
}

void AHttpPoseScoreActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshTextRender();
}

void AHttpPoseScoreActor::UpdatePoseData(const FString& Message, double Confidence)
{
	LastMessage = Message;
	LastConfidence = Confidence;
	RefreshTextRender();

	if (GEngine)
	{
		const FString DebugText = FString::Printf(TEXT("Score Updated: %s (Confidence %.3f)"), *LastMessage, LastConfidence);
		GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this), 5.f, FColor::Green, DebugText);
	}
}

void AHttpPoseScoreActor::SetDebugText(const FString& InText)
{
	LastMessage = InText;
	LastConfidence = 0.0;
	RefreshTextRender();
}

void AHttpPoseScoreActor::RefreshTextRender()
{
	if (!TextComponent)
	{
		return;
	}

	const FString DisplayString = FString::Printf(TEXT("%s\nConfidence: %.3f"), *LastMessage, LastConfidence);
	TextComponent->SetText(FText::FromString(DisplayString));
	TextComponent->SetTextRenderColor(TextColor.ToFColor(true));
	TextComponent->SetWorldSize(TextSize);
	TextComponent->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	TextComponent->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
	TextComponent->SetWorldRotation(FRotator::ZeroRotator);

	TextComponent->bAlwaysRenderAsText = true;
}
