#pragma once

#include "CityGeneratorTool.h"
#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Content of the "Generador de ciudades" tab: the six parameters, Generate and Cleanup.
 * The scenario engineers use only this panel; they never open a PCG graph.
 */
class SCityGeneratorPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCityGeneratorPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FCityGeneratorTool>, Tool)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Refreshes the statistics twice per second while the tab is visible. */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	/** A label and a spin box bound to one of the tool parameters. */
	template <typename T>
	TSharedRef<SWidget> MakeParameterRow(const FText& Label, const FText& Tooltip, T& Value, T MinValue, T MaxValue, T Delta);
	TSharedRef<SWidget> MakeSectionHeader(const FText& Title) const;

	FReply OnGenerateClicked();
	FReply OnCleanupClicked();
	FReply OnRandomSeedClicked();
	/** A value was typed or a slider released: regenerate if live updates are on. */
	void OnParameterCommitted();
	FText GetGraphStatusText() const;
	FText GetStatsText() const;

	TSharedPtr<FCityGeneratorTool> Tool;
	FText StatusText;
	FCityStats CachedStats;
	double NextStatsRefreshTime = 0.0;
	bool bAutoRegenerate = false;

	mutable bool bGraphAvailable = false;
	mutable double NextGraphCheckTime = 0.0;
};
