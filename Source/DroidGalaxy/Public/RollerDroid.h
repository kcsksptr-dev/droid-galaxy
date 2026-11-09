#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RollerDroid.generated.h"

class AAlienPlanet;
class UCameraComponent;
class UMaterialInstanceDynamic;
class UPhysicalMaterial;
class USphereComponent;
class USpringArmComponent;
class UStaticMeshComponent;

/**
 * KDS-3-21 (model "Pip"): a rolling ball droid with a free-floating dome head.
 *
 * Works on flat maps (world Z up) and on an AAlienPlanet (radial gravity: "up" is away from the planet
 * centre, gravity is applied by the droid itself). The camera keeps its own tangent-frame heading, so it
 * never flips when driving over the poles.
 *
 * The root is a physics-simulated sphere driven by torque (camera-relative WASD), so it really rolls,
 * bumps and tumbles down slopes. The body mesh rolls with it; the head uses an absolute transform and is
 * placed on top every frame after physics: it faces the travel direction, leans with acceleration,
 * bobs when idle and turns to look at the camera when left alone. Glow materials pulse and blink.
 *
 * Health: hazards (AHazard) hurt it; at 0 HP it reboots at the ship. Power-ups: repair kit, shield bubble
 * (blocks damage for ShieldDuration), blaster (shoots cacti/thorns/walls for BlasterDuration).
 *
 * Controls: WASD/left stick roll, mouse look, Shift boost, Ctrl careful (slow), Space jump, E interact,
 * left mouse fire (with blaster), mouse wheel zoom.
 */
UCLASS(Config = Game)
class DROIDGALAXY_API ARollerDroid : public APawn
{
	GENERATED_BODY()

public:
	ARollerDroid();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;

	/** Console: drive by itself for Seconds with the given forward/right input (testing, demos). */
	UFUNCTION(Exec)
	void PipDrive(float Seconds, float Forward, float Right);

	/** Scripted self-test (started by the -PipTest command line flag): drives, jumps, idles,
	 *  logs "[PipTest]" telemetry, takes HighResShot screenshots, then quits. */
	UFUNCTION(Exec)
	void PipTestRun();

	/** Pick up the part in reach / install parts at the ship / launch the repaired ship. */
	void Interact();

	/** What E would do right now, empty if nothing is in reach (for the HUD). */
	FString GetInteractPrompt() const;

	const TArray<int32>& GetInventory() const { return Inventory; }
	FVector GetUp() const;
	FVector GetCameraForward() const { return CamForward; }
	AAlienPlanet* GetPlanet() const { return Planet; }

	void ShowMessage(const FString& Text, float Seconds = 3.f);
	FString GetMessage() const { return Clock < MessageUntil ? Message : FString(); }
	float GetMessageAge() const { return Clock - MessageStart; }

	/** Board the ship: hide the droid, stop driving and keep the camera on the target. */
	void Board(AActor* WatchTarget);

	static const TCHAR* DroidName() { return TEXT("KDS-3-21"); }

	/** Damage from a hazard (blocked by the shield, short invulnerability after a hit). VelocityChange is the knockback. */
	void ApplyHazardDamage(float Amount, const FVector& Source, const FString& Reason, const FVector& VelocityChange);

	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	float GetShieldTime() const { return ShieldTime; }
	float GetBlasterTime() const { return BlasterTime; }
	bool IsCareful() const { return bCareful; }
	bool IsKnockedOut() const { return bKnockedOut; }
	/** 1 right after taking damage, fading to 0 (HUD flash). */
	float GetDamageFlash() const { return FMath::Clamp(1.f - (Clock - LastDamageTime) / 0.6f, 0.f, 1.f); }
	/** Where blaster shots go (unit vector). */
	FVector GetAimDirection() const;
	FVector GetMuzzleLocation() const;

	UFUNCTION(Exec)
	void PipFire();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Droid")
	TObjectPtr<USphereComponent> Ball;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Droid")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Droid")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Droid")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Droid")
	TObjectPtr<UCameraComponent> Camera;

	/** Angular acceleration applied while rolling on the ground (rad/s^2). */
	UPROPERTY(EditAnywhere, Category = "Droid|Movement")
	float RollAcceleration = 45.f;

	/** Top rolling speed without boost (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Droid|Movement")
	float MaxSpeed = 650.f;

	UPROPERTY(EditAnywhere, Category = "Droid|Movement")
	float BoostMultiplier = 1.7f;

	/** Horizontal acceleration while airborne, as a fraction of the ground acceleration. */
	UPROPERTY(EditAnywhere, Category = "Droid|Movement")
	float AirControl = 0.3f;

	/** How fast spin is braked when there is no input on the ground (1/s); keeps Pip parked on slopes. */
	UPROPERTY(EditAnywhere, Category = "Droid|Movement")
	float BrakeRate = 3.f;

	/** Below this horizontal speed (cm/s) with no input, Pip parks: strong brake on spin and slide. */
	UPROPERTY(EditAnywhere, Category = "Droid|Movement")
	float ParkBelowSpeed = 150.f;

	UPROPERTY(EditAnywhere, Category = "Droid|Movement")
	float ParkBrakeRate = 14.f;

	/** Upward velocity change of a jump (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Droid|Movement")
	float JumpSpeed = 520.f;

	/** Gap between the top of the ball and the head base (cm). */
	UPROPERTY(EditAnywhere, Category = "Droid|Head")
	float HeadGap = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Droid|Head")
	float HeadTurnSpeed = 6.f;

	UPROPERTY(EditAnywhere, Category = "Droid|Head")
	float MaxLeanDegrees = 20.f;

	/** Lean against acceleration, degrees per cm/s^2. */
	UPROPERTY(EditAnywhere, Category = "Droid|Head")
	float LeanPerAccel = 0.012f;

	/** Lean into the direction of travel, degrees per cm/s. */
	UPROPERTY(EditAnywhere, Category = "Droid|Head")
	float LeanPerSpeed = 0.012f;

	/** Seconds standing still before Pip turns round to look at the camera. */
	UPROPERTY(EditAnywhere, Category = "Droid|Head")
	float CuriousAfter = 3.f;

	UPROPERTY(EditAnywhere, Category = "Droid|Glow")
	float GlowPulseSpeed = 0.8f;

	UPROPERTY(EditAnywhere, Category = "Droid|Glow")
	float GlowPulseAmount = 0.3f;

	UPROPERTY(EditAnywhere, Category = "Droid|Health")
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, Category = "Droid|Health")
	float ShieldDuration = 20.f;

	UPROPERTY(EditAnywhere, Category = "Droid|Health")
	float BlasterDuration = 30.f;

	/** Top speed while holding the careful key (cm/s); slow enough for thorn fields. */
	UPROPERTY(EditAnywhere, Category = "Droid|Movement")
	float CarefulSpeed = 190.f;

	UPROPERTY(EditAnywhere, Category = "Droid|Mission")
	float PickupRange = 280.f;

	UPROPERTY(EditAnywhere, Category = "Droid|Mission")
	float ShipRange = 420.f;

private:
	void MoveForward(float Value);
	void MoveRight(float Value);
	void Turn(float Value);
	void LookUp(float Value);
	void StartBoost();
	void StopBoost();
	void Jump();
	void ZoomIn();
	void ZoomOut();
	void StartCareful();
	void StopCareful();
	void UpdateHazards(float DeltaSeconds);
	void UpdatePowerUps(float DeltaSeconds);
	void KnockOut();
	void Reboot();

	bool IsGrounded() const;
	void ApplySpeedLimit();
	void UpdateCamera(float DeltaSeconds);
	void UpdateHead(float DeltaSeconds);
	AActor* FindInteractable() const;
	void TeleportNear(const FVector& Target, float Distance);
	void UpdateGlow(float DeltaSeconds);

	static void InitInputBindings();

	UPROPERTY(Transient)
	TObjectPtr<UPhysicalMaterial> BallPhysMaterial;

	UPROPERTY(Transient)
	TObjectPtr<AAlienPlanet> Planet;

	UPROPERTY(Transient)
	TObjectPtr<AActor> WatchTarget;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ShieldBubble;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ShieldMaterial;

	float Health = 100.f;
	float ShieldTime = 0.f;
	float BlasterTime = 0.f;
	float NextFireTime = 0.f;
	float InvulnerableUntil = 0.f;
	float LastDamageTime = -10.f;
	float NextLavaBurn = 0.f;
	bool bCareful = false;
	bool bKnockedOut = false;

	TArray<int32> Inventory;
	FString Message;
	float MessageUntil = 0.f;
	float MessageStart = 0.f;
	bool bBoarded = false;
	FVector CamForward = FVector::ForwardVector;
	float CamPitch = -12.f;
	float PendingYaw = 0.f;
	float PendingPitch = 0.f;
	FVector HeadForward = FVector::ForwardVector;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> GlowMaterials;

	TArray<float> GlowBaseStrength;

	FVector2D MoveInput = FVector2D::ZeroVector;
	bool bBoost = false;
	FVector PrevVelocity = FVector::ZeroVector;
	FVector SmoothedAccel = FVector::ZeroVector;
	FQuat HeadLean = FQuat::Identity;
	float Clock = 0.f;
	float IdleTime = 0.f;
	float GlanceYaw = 0.f;
	float NextGlanceTime = 0.f;
	float NextBlinkTime = 3.f;
	float BlinkUntil = 0.f;
	FVector2D AutoDriveInput = FVector2D::ZeroVector;
	float AutoDriveUntil = -1.f;
	float NextTelemetryTime = 0.f;
	bool bTestRunning = false;
};
