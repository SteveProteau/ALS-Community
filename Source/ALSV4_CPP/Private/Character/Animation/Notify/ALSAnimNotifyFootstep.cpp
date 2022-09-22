// Copyright:       Copyright (C) 2022 Doğa Can Yanıkoğlu
// Source Code:     https://github.com/dyanikoglu/ALS-Community


#include "Character/Animation/Notify/ALSAnimNotifyFootstep.h"

#include "Components/AudioComponent.h"
#include "Engine/DataTable.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Library/ALSCharacterStructLibrary.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

#include "Engine/AssetManager.h"

void UALSAnimNotifyFootstep::PostLoad()
{
	Super::PostLoad();

	// Preload footstep assets asynchronously
	if (HitDataTable)
	{
		TArray<FALSHitFX*> HitFXRows;
		HitDataTable->GetAllRows<FALSHitFX>(FString(), HitFXRows);

		for (auto row : HitFXRows)
		{
			UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(row->Sound.ToSoftObjectPath(), [](){});
			UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(row->NiagaraSystem.ToSoftObjectPath(), [](){});
			UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(row->DecalMaterial.ToSoftObjectPath(), [](){});
		}
	}
}


const FName NAME_Mask_FootstepSound(TEXT("Mask_FootstepSound"));

FName UALSAnimNotifyFootstep::NAME_FootstepType(TEXT("FootstepType"));
FName UALSAnimNotifyFootstep::NAME_Foot_R(TEXT("Foot_R"));

#if 0 
void UALSAnimNotifyFootstep::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	AActor* MeshOwner = MeshComp->GetOwner();
	if (!MeshOwner)
	{
		return;
	}

	if (HitDataTable)
	{
		UWorld* World = MeshComp->GetWorld();
		check(World);

		const FVector FootLocation = MeshComp->GetSocketLocation(FootSocketName);
		const FRotator FootRotation = MeshComp->GetSocketRotation(FootSocketName);
		const FVector TraceEnd = FootLocation - MeshOwner->GetActorUpVector() * TraceLength;

		FHitResult Hit;

		if (UKismetSystemLibrary::LineTraceSingle(MeshOwner /*used by bIgnoreSelf*/, FootLocation, TraceEnd, TraceChannel, true /*bTraceComplex*/, MeshOwner->Children,
		                                          DrawDebugType, Hit, true /*bIgnoreSelf*/))
		{
			if (!Hit.PhysMaterial.Get())
			{
				return;
			}

			const EPhysicalSurface SurfaceType = Hit.PhysMaterial.Get()->SurfaceType;

			check(IsInGameThread());
			checkNoRecursion();
			static TArray<FALSHitFX*> HitFXRows;
			HitFXRows.Reset();

			HitDataTable->GetAllRows<FALSHitFX>(FString(), HitFXRows);

			FALSHitFX* HitFX = nullptr;
			if (auto FoundResult = HitFXRows.FindByPredicate([&](const FALSHitFX* Value)
			{
				return SurfaceType == Value->SurfaceType;
			}))
			{
				HitFX = *FoundResult;
			}
			else if (auto DefaultResult = HitFXRows.FindByPredicate([&](const FALSHitFX* Value)
			{
				return EPhysicalSurface::SurfaceType_Default == Value->SurfaceType;
			}))
			{
				HitFX = *DefaultResult;
			}
			else
			{
				return;
			}

			if (bSpawnSound && HitFX->Sound.LoadSynchronous())
			{
				UAudioComponent* SpawnedSound = nullptr;

				const float MaskCurveValue = MeshComp->GetAnimInstance()->GetCurveValue(
					NAME_Mask_FootstepSound);
				const float FinalVolMult = bOverrideMaskCurve
					                           ? VolumeMultiplier
					                           : VolumeMultiplier * (1.0f - MaskCurveValue);

				switch (HitFX->SoundSpawnType)
				{
				case EALSSpawnType::Location:
					SpawnedSound = UGameplayStatics::SpawnSoundAtLocation(
						World, HitFX->Sound.Get(), Hit.Location + HitFX->SoundLocationOffset,
						HitFX->SoundRotationOffset, FinalVolMult, PitchMultiplier);
					break;

				case EALSSpawnType::Attached:
					SpawnedSound = UGameplayStatics::SpawnSoundAttached(HitFX->Sound.Get(), MeshComp, FootSocketName,
					                                                    HitFX->SoundLocationOffset,
					                                                    HitFX->SoundRotationOffset,
					                                                    HitFX->SoundAttachmentType, true, FinalVolMult,
					                                                    PitchMultiplier);

					break;
				}
				if (SpawnedSound)
				{
					SpawnedSound->SetIntParameter(SoundParameterName, static_cast<int32>(FootstepType));
				}
			}

			if (bSpawnNiagara && HitFX->NiagaraSystem.LoadSynchronous())
			{
				UNiagaraComponent* SpawnedParticle = nullptr;
				const FVector Location = Hit.Location + MeshOwner->GetTransform().TransformVector(
					HitFX->DecalLocationOffset);

				switch (HitFX->NiagaraSpawnType)
				{
				case EALSSpawnType::Location:
					SpawnedParticle = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
						World, HitFX->NiagaraSystem.Get(), Location, FootRotation + HitFX->NiagaraRotationOffset);
					break;

				case EALSSpawnType::Attached:
					SpawnedParticle = UNiagaraFunctionLibrary::SpawnSystemAttached(
						HitFX->NiagaraSystem.Get(), MeshComp, FootSocketName, HitFX->NiagaraLocationOffset,
						HitFX->NiagaraRotationOffset, HitFX->NiagaraAttachmentType, true);
					break;
				}
			}

			if (bSpawnDecal && HitFX->DecalMaterial.LoadSynchronous())
			{
				const FVector Location = Hit.Location + MeshOwner->GetTransform().TransformVector(
					HitFX->DecalLocationOffset);

				const FVector DecalSize = FVector(bMirrorDecalX ? -HitFX->DecalSize.X : HitFX->DecalSize.X,
				                                  bMirrorDecalY ? -HitFX->DecalSize.Y : HitFX->DecalSize.Y,
				                                  bMirrorDecalZ ? -HitFX->DecalSize.Z : HitFX->DecalSize.Z);

				UDecalComponent* SpawnedDecal = nullptr;
				switch (HitFX->DecalSpawnType)
				{
				case EALSSpawnType::Location:
					SpawnedDecal = UGameplayStatics::SpawnDecalAtLocation(
						World, HitFX->DecalMaterial.Get(), DecalSize, Location,
						FootRotation + HitFX->DecalRotationOffset, HitFX->DecalLifeSpan);
					break;

				case EALSSpawnType::Attached:
					SpawnedDecal = UGameplayStatics::SpawnDecalAttached(HitFX->DecalMaterial.Get(), DecalSize,
					                                                    Hit.Component.Get(), NAME_None, Location,
					                                                    FootRotation + HitFX->DecalRotationOffset,
					                                                    HitFX->DecalAttachmentType,
					                                                    HitFX->DecalLifeSpan);
					break;
				}
			}
		}
	}
}
#endif //Disabled

FString UALSAnimNotifyFootstep::GetNotifyName_Implementation() const
{
	FString Name(TEXT("Footstep Type: "));
	Name.Append(GetEnumerationToString(FootstepType));
	return Name;
}

FCollisionQueryParams ConfigureCollisionParams(FName TraceTag, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, bool bIgnoreSelf, const UObject* WorldContextObject)
{
	FCollisionQueryParams Params(TraceTag, SCENE_QUERY_STAT_ONLY(KismetTraceUtils), bTraceComplex);
	Params.bReturnPhysicalMaterial = true;
	Params.bReturnFaceIndex = false; //!UPhysicsSettings::Get()->bSuppressFaceRemapTable; // Ask for face index, as long as we didn't disable globally
	Params.AddIgnoredActors(ActorsToIgnore);
	if (bIgnoreSelf)
	{
		const AActor* IgnoreActor = Cast<AActor>(WorldContextObject);
		if (IgnoreActor)
		{
			Params.AddIgnoredActor(IgnoreActor);
		}
		else
		{
			// find owner
			const UObject* CurrentObject = WorldContextObject;
			while (CurrentObject)
			{
				CurrentObject = CurrentObject->GetOuter();
				IgnoreActor = Cast<AActor>(CurrentObject);
				if (IgnoreActor)
				{
					Params.AddIgnoredActor(IgnoreActor);
					break;
				}
			}
		}
	}

	return Params;
}


void UALSAnimNotifyFootstep::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	AActor* MeshOwner = MeshComp->GetOwner();
	if (!MeshOwner)
	{
		return;
	}
	
	if (HitDataTable)
	{
		UWorld* World = MeshComp->GetWorld();
		check(World);

		const FVector FootLocation = MeshComp->GetSocketLocation(FootSocketName);
		const FRotator FootRotation = MeshComp->GetSocketRotation(FootSocketName);
		const FVector TraceEnd = FootLocation - MeshOwner->GetActorUpVector() * TraceLength;
		float WaterDepth = 0;

		FHitResult Hit;

		if (UKismetSystemLibrary::LineTraceSingle(MeshOwner /*used by bIgnoreSelf*/, FootLocation, TraceEnd, TraceChannel, true /*bTraceComplex*/, MeshOwner->Children,
			DrawDebugType, Hit, true /*bIgnoreSelf*/))
		{
			if (!Hit.PhysMaterial.Get())
			{
				return;
			}

			EPhysicalSurface SurfaceType = Hit.PhysMaterial.Get()->SurfaceType;
			FVector HitLocation = Hit.Location;

			//
			// Trace the CustomWater channel check to see if we are in water
			//
			ETraceTypeQuery CustomWaterChannel = TraceTypeQuery3;

			FHitResult HitWater;
			FVector TraceStartWater = MeshComp->GetSocketLocation(FootSocketName) + MeshOwner->GetActorUpVector() * 130.0f;
			FVector TraceEndWater = FootLocation - MeshOwner->GetActorUpVector() * 150.0f;

			if (UKismetSystemLibrary::LineTraceSingle(MeshOwner /*used by bIgnoreSelf*/, TraceStartWater, TraceEndWater, CustomWaterChannel, true /*bTraceComplex*/, MeshOwner->Children,
				DrawDebugType, HitWater, true /*bIgnoreSelf*/))
			{
				if (HitWater.PhysMaterial.Get())
				{
					EPhysicalSurface WaterTraceSurfaceType = HitWater.PhysMaterial.Get()->SurfaceType;
					if (WaterTraceSurfaceType == SurfaceType1 /*Water*/)
					{
						WaterDepth = HitWater.Location.Z - HitLocation.Z;
						if (WaterDepth > -0.5f)
						{
							// Clamp the water depth to reasonable values
							if (WaterDepth < 0)
							{
								WaterDepth = 0;
							}
							else if (WaterDepth > 200)
							{
								WaterDepth = 200;
							}

							// Redirect the hit location to the water surface 
							HitLocation = HitWater.Location;

							// Change the surface type based on the water depth
							if (WaterDepth < 5)
							{
								// WaterPuddle
								SurfaceType = SurfaceType1;
							}
							else if (WaterDepth < 15)
							{
								// WaterAnkleLevel
								SurfaceType = SurfaceType9;
							}
							else if (WaterDepth < 35)
							{
								// WaterKneeLevel
								SurfaceType = SurfaceType10;
							}
							else if (WaterDepth < 75)
							{
								// WaterThighLevel
								SurfaceType = SurfaceType11;
							}
							else
							{
								// WaterFullBody
								SurfaceType = SurfaceType12;
							}
						}
					}
				}
			}

			check(IsInGameThread());
			checkNoRecursion();
			static TArray<FALSHitFX*> HitFXRows;
			HitFXRows.Reset();

			HitDataTable->GetAllRows<FALSHitFX>(FString(), HitFXRows);

			FALSHitFX* HitFX = nullptr;
			if (auto FoundResult = HitFXRows.FindByPredicate([&](const FALSHitFX* Value)
			{
				return SurfaceType == Value->SurfaceType;
			}))
			{
				HitFX = *FoundResult;
			}
			else if (auto DefaultResult = HitFXRows.FindByPredicate([&](const FALSHitFX* Value)
			{
				return EPhysicalSurface::SurfaceType_Default == Value->SurfaceType;
			}))
			{
				HitFX = *DefaultResult;
			}
			else
			{
				return;
			}

			//if (bSpawnSound && HitFX->Sound.LoadSynchronous())
			if (bSpawnSound && HitFX->Sound.IsValid())
			{
				UAudioComponent* SpawnedSound = nullptr;

				const float MaskCurveValue = MeshComp->GetAnimInstance()->GetCurveValue(
					NAME_Mask_FootstepSound);
				const float FinalVolMult = bOverrideMaskCurve
					? VolumeMultiplier
					: VolumeMultiplier * (1.0f - MaskCurveValue);

				switch (HitFX->SoundSpawnType)
				{
				case EALSSpawnType::Location:
					SpawnedSound = UGameplayStatics::SpawnSoundAtLocation(
						World, HitFX->Sound.Get(), Hit.Location + HitFX->SoundLocationOffset,
						HitFX->SoundRotationOffset, FinalVolMult, PitchMultiplier);
					break;

				case EALSSpawnType::Attached:
					SpawnedSound = UGameplayStatics::SpawnSoundAttached(HitFX->Sound.Get(), MeshComp, FootSocketName,
						HitFX->SoundLocationOffset,
						HitFX->SoundRotationOffset,
						HitFX->SoundAttachmentType, true, FinalVolMult,
						PitchMultiplier);

					break;
				}
				if (SpawnedSound)
				{
					SpawnedSound->SetIntParameter(SoundParameterName, static_cast<int32>(FootstepType));
				}
			}

			//if (bSpawnNiagara && HitFX->NiagaraSystem.LoadSynchronous())
			if (bSpawnNiagara && HitFX->NiagaraSystem.IsValid())
			{
				UNiagaraComponent* SpawnedParticle = nullptr;
				const FVector Location = HitLocation + MeshOwner->GetTransform().TransformVector(
					HitFX->DecalLocationOffset);

				switch (HitFX->NiagaraSpawnType)
				{
				case EALSSpawnType::Location:
				{
					FRotator NoFootRotation;
					SpawnedParticle = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
						World, HitFX->NiagaraSystem.Get(), Location, NoFootRotation /*FootRotation + HitFX->NiagaraRotationOffset*/);
					break;
				}
				case EALSSpawnType::Attached:
					SpawnedParticle = UNiagaraFunctionLibrary::SpawnSystemAttached(
						HitFX->NiagaraSystem.Get(), MeshComp, FootSocketName, HitFX->NiagaraLocationOffset,
						HitFX->NiagaraRotationOffset, HitFX->NiagaraAttachmentType, true);
					break;
				}

				// Adjust water splash size based on water depth
				if (SpawnedParticle && WaterDepth > 5)
				{
					SpawnedParticle->SetIntParameter("NumberOfPaRTICLES", WaterDepth / 3);
				}
			}

			//if (bSpawnDecal && HitFX->DecalMaterial.LoadSynchronous())
			if (bSpawnDecal && HitFX->DecalMaterial.IsValid())
			{
				const FVector Location = HitLocation + MeshOwner->GetTransform().TransformVector(
					HitFX->DecalLocationOffset);

				const FVector DecalSize = FVector(bMirrorDecalX ? -HitFX->DecalSize.X : HitFX->DecalSize.X,
					bMirrorDecalY ? -HitFX->DecalSize.Y : HitFX->DecalSize.Y,
					bMirrorDecalZ ? -HitFX->DecalSize.Z : HitFX->DecalSize.Z);

				UDecalComponent* SpawnedDecal = nullptr;
				switch (HitFX->DecalSpawnType)
				{
				case EALSSpawnType::Location:
					SpawnedDecal = UGameplayStatics::SpawnDecalAtLocation(
						World, HitFX->DecalMaterial.Get(), DecalSize, Location,
						FootRotation + HitFX->DecalRotationOffset, HitFX->DecalLifeSpan);
					break;

				case EALSSpawnType::Attached:
					SpawnedDecal = UGameplayStatics::SpawnDecalAttached(HitFX->DecalMaterial.Get(), DecalSize,
						Hit.Component.Get(), NAME_None, Location,
						FootRotation + HitFX->DecalRotationOffset,
						HitFX->DecalAttachmentType,
						HitFX->DecalLifeSpan);
					break;
				}
			}
		}
	}
}
