#pragma once

#include "CityGenerator.h"
#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Content of the "Generador de ciudades" tab: the six parameters, Generate and Cleanup, the
 * messages and the statistics of the city in the level. The scenario engineers only use this
 * panel; they never open a PCG graph.
 */
class SCityPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCityPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FCityGenerator>, Generator)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Refreshes the statistics twice per second while the tab is visible. */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	/** A label and a spin box bound to one of the parameters. */
	template <typename T>
	TSharedRef<SWidget> MakeParameterRow(const FText& Label, const FText& Tooltip, T& Value, T MinValue, T MaxValue, T Delta);
	TSharedRef<SWidget> MakeSectionHeader(const FText& Title) const;

	FReply OnGenerateClicked();
	FReply OnCleanupClicked();
	FReply OnRandomSeedClicked();
	FReply OnRebuildGraphsClicked();
	FReply OnCopyMessagesClicked();

	/** A value was typed or a slider released: regenerate if live updates are on. */
	void OnParameterCommitted();

	FText GetGridText() const;
	FText GetMessagesText() const;
	FText GetStatsText() const;

	TSharedPtr<FCityGenerator> Generator;
	FText StatusText;
	FCityStats CachedStats;
	double NextStatsRefreshTime = 0.0;
	bool bAutoRegenerate = false;
};
