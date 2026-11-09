#include "RollerDroid.h"

#include "AlienPlanet.h"
#include "BlasterBolt.h"
#include "Hazard.h"
#include "PowerUp.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "MissionTypes.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ShipPart.h"
#include "Spaceship.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float BallRadius = 28.f; // matches SM_PipBody (Tools/make_droid.py BODY_R = 0.28 m)
	constexpr float MouseDegreesPerUnit = 2.5f;
	const FName EmissiveParam(TEXT("EmissiveStrength"));

	/** Signed angle (radians) from A to B around Axis; A and B are in the plane perpendicular to Axis. */
	float SignedAngle(const FVector& A, const FVector& B, const FVector& Axis)
	{
		return FMath::Atan2(FVector::DotProduct(FVector::CrossProduct(A, B), Axis), FVector::DotProduct(A, B));
	}

	FVector Tangent(const FVector& V, const FVector& Up, const FVector& Fallback)
	{
		FVector T = FVector::VectorPlaneProject(V, Up);
		if (!T.Normalize())
		{
			T = FVector::VectorPlaneProject(Fallback, Up);
			if (!T.Normalize())
			{
				T = FVector::VectorPlaneProject(FVector::RightVector, Up).GetSafeNormal();
			}
		}
		return T;
	}
}

ARollerDroid::ARollerDroid()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics; // head + camera follow the simulated ball

	// The ball's rotation belongs to physics; never let the controller overwrite it.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	Ball = CreateDefaultSubobject<USphereComponent>(TEXT("Ball"));
	Ball->InitSphereRadius(BallRadius);
	Ball->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Ball->SetSimulatePhysics(true);
	Ball->SetMassOverrideInKg(NAME_None, 35.f, true);
	Ball->SetLinearDamping(0.15f);
	Ball->SetAngularDamping(0.8f);
	Ball->BodyInstance.bUseCCD = true;
	Ball->SetCanEverAffectNavigation(false);
	RootComponent = Ball;

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	BodyMesh->SetupAttachment(Ball);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetCanEverAffectNavigation(false);

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	HeadMesh->SetupAttachment(Ball);
	HeadMesh->SetUsingAbsoluteLocation(true); // placed manually every frame, never rolls with the body
	HeadMesh->SetUsingAbsoluteRotation(true);
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyAsset(TEXT("/Game/Characters/Pip/SM_PipBody.SM_PipBody"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> HeadAsset(TEXT("/Game/Characters/Pip/SM_PipHead.SM_PipHead"));
	if (BodyAsset.Succeeded())
	{
		BodyMesh->SetStaticMesh(BodyAsset.Object);
	}
	if (HeadAsset.Succeeded())
	{
		HeadMesh->SetStaticMesh(HeadAsset.Object);
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Ball);
	CameraBoom->SetUsingAbsoluteRotation(true); // rotation set every tick from the droid's own camera frame
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->TargetArmLength = 340.f;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 10.f;
	CameraBoom->ProbeSize = 14.f;
	CameraBoom->PrimaryComponentTick.TickGroup = TG_PostPhysics;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
	Camera->FieldOfView = 80.f;

	InitInputBindings();
}

void ARollerDroid::InitInputBindings()
{
	static bool bBindingsAdded = false;
	if (bBindingsAdded)
	{
		return;
	}
	bBindingsAdded = true;

	auto Axis = [](const TCHAR* Name, const FKey& Key, float Scale)
	{ UPlayerInput::AddEngineDefinedAxisMapping(FInputAxisKeyMapping(Name, Key, Scale)); };
	auto Action = [](const TCHAR* Name, const FKey& Key)
	{ UPlayerInput::AddEngineDefinedActionMapping(FInputActionKeyMapping(Name, Key)); };

	Axis(TEXT("Droid_MoveForward"), EKeys::W, 1.f);
	Axis(TEXT("Droid_MoveForward"), EKeys::S, -1.f);
	Axis(TEXT("Droid_MoveForward"), EKeys::Up, 1.f);
	Axis(TEXT("Droid_MoveForward"), EKeys::Down, -1.f);
	Axis(TEXT("Droid_MoveForward"), EKeys::Gamepad_LeftY, 1.f);
	Axis(TEXT("Droid_MoveRight"), EKeys::D, 1.f);
	Axis(TEXT("Droid_MoveRight"), EKeys::A, -1.f);
	Axis(TEXT("Droid_MoveRight"), EKeys::Right, 1.f);
	Axis(TEXT("Droid_MoveRight"), EKeys::Left, -1.f);
	Axis(TEXT("Droid_MoveRight"), EKeys::Gamepad_LeftX, 1.f);
	Axis(TEXT("Droid_Turn"), EKeys::MouseX, 1.f);
	Axis(TEXT("Droid_Turn"), EKeys::Gamepad_RightX, 1.5f);
	Axis(TEXT("Droid_LookUp"), EKeys::MouseY, -1.f);
	Axis(TEXT("Droid_LookUp"), EKeys::Gamepad_RightY, -1.5f);

	Action(TEXT("Droid_Jump"), EKeys::SpaceBar);
	Action(TEXT("Droid_Jump"), EKeys::Gamepad_FaceButton_Bottom);
	Action(TEXT("Droid_Boost"), EKeys::LeftShift);
	Action(TEXT("Droid_Boost"), EKeys::Gamepad_RightTrigger);
	Action(TEXT("Droid_Careful"), EKeys::LeftControl);
	Action(TEXT("Droid_Careful"), EKeys::Gamepad_LeftShoulder);
	Action(TEXT("Droid_Fire"), EKeys::LeftMouseButton);
	Action(TEXT("Droid_Fire"), EKeys::Gamepad_RightShoulder);
	Action(TEXT("Droid_Interact"), EKeys::E);
	Action(TEXT("Droid_Interact"), EKeys::Gamepad_FaceButton_Left);
	Action(TEXT("Droid_ZoomIn"), EKeys::MouseScrollUp);
	Action(TEXT("Droid_ZoomOut"), EKeys::MouseScrollDown);
}

void ARollerDroid::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PlayerInputComponent->BindAxis(TEXT("Droid_MoveForward"), this, &ARollerDroid::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("Droid_MoveRight"), this, &ARollerDroid::MoveRight);
	PlayerInputComponent->BindAxis(TEXT("Droid_Turn"), this, &ARollerDroid::Turn);
	PlayerInputComponent->BindAxis(TEXT("Droid_LookUp"), this, &ARollerDroid::LookUp);
	PlayerInputComponent->BindAction(TEXT("Droid_Jump"), IE_Pressed, this, &ARollerDroid::Jump);
	PlayerInputComponent->BindAction(TEXT("Droid_Boost"), IE_Pressed, this, &ARollerDroid::StartBoost);
	PlayerInputComponent->BindAction(TEXT("Droid_Boost"), IE_Released, this, &ARollerDroid::StopBoost);
	PlayerInputComponent->BindAction(TEXT("Droid_Interact"), IE_Pressed, this, &ARollerDroid::Interact);
	PlayerInputComponent->BindAction(TEXT("Droid_Careful"), IE_Pressed, this, &ARollerDroid::StartCareful);
	PlayerInputComponent->BindAction(TEXT("Droid_Careful"), IE_Released, this, &ARollerDroid::StopCareful);
	PlayerInputComponent->BindAction(TEXT("Droid_Fire"), IE_Pressed, this, &ARollerDroid::PipFire);
	PlayerInputComponent->BindAction(TEXT("Droid_ZoomIn"), IE_Pressed, this, &ARollerDroid::ZoomIn);
	PlayerInputComponent->BindAction(TEXT("Droid_ZoomOut"), IE_Pressed, this, &ARollerDroid::ZoomOut);
}

void ARollerDroid::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	CamForward = Tangent(GetActorForwardVector(), GetUp(), FVector::ForwardVector);
	HeadForward = CamForward;
}

void ARollerDroid::BeginPlay()
{
	Super::BeginPlay();

	Planet = AAlienPlanet::Find(this);
	Health = MaxHealth;
	if (Planet)
	{
		Ball->SetEnableGravity(false); // radial gravity is applied in Tick
		if (Planet->EffectSphereMesh && Planet->BeaconMaterial)
		{
			ShieldBubble = NewObject<UStaticMeshComponent>(this, TEXT("ShieldBubble"));
			ShieldBubble->SetStaticMesh(Planet->EffectSphereMesh);
			ShieldBubble->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ShieldBubble->SetCastShadow(false);
			ShieldBubble->SetUsingAbsoluteLocation(true);
			ShieldBubble->SetUsingAbsoluteRotation(true);
			ShieldBubble->SetWorldScale3D(FVector(1.25f));
			ShieldBubble->SetupAttachment(Ball);
			ShieldBubble->RegisterComponent();
			ShieldMaterial = UMaterialInstanceDynamic::Create(Planet->BeaconMaterial, this);
			ShieldBubble->SetMaterial(0, ShieldMaterial);
			ShieldBubble->SetVisibility(false);
		}
	}

	// Grippy, slightly bouncy rubber so torque turns into rolling instead of spinning in place.
	BallPhysMaterial = NewObject<UPhysicalMaterial>(this, TEXT("PipRubber"));
	BallPhysMaterial->Friction = 1.2f;
	BallPhysMaterial->Restitution = 0.15f;
	BallPhysMaterial->bOverrideFrictionCombineMode = true;
	BallPhysMaterial->FrictionCombineMode = EFrictionCombineMode::Max;
	Ball->SetPhysMaterialOverride(BallPhysMaterial);
	ApplySpeedLimit();

	CamForward = Tangent(GetActorForwardVector(), GetUp(), FVector::ForwardVector);
	HeadForward = CamForward;
	PrevVelocity = FVector::ZeroVector;
	// the boom must read this tick's camera rotation
	CameraBoom->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);

	// Dynamic instances of every glowing slot so the eyes/antenna tips can pulse and blink.
	for (UStaticMeshComponent* Mesh : {BodyMesh.Get(), HeadMesh.Get()})
	{
		for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
		{
			UMaterialInterface* Material = Mesh->GetMaterial(Index);
			if (Material && (Material->GetName().Contains(TEXT("Glow")) || Material->GetName().Contains(TEXT("Spark"))))
			{
				UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(Index, Material);
				float Base = 0.f;
				MID->GetScalarParameterValue(FHashedMaterialParameterInfo(EmissiveParam), Base);
				GlowMaterials.Add(MID);
				GlowBaseStrength.Add(Base);
			}
		}
	}

	// next tick: the planet (whose BeginPlay may run after ours) has spawned its parts by then
	GetWorldTimerManager().SetTimerForNextTick([this]
	{
		if (Planet)
		{
			ShowMessage(FString::Printf(TEXT("%s: crash-landed on %s. Find the 5 lost ship parts!"), DroidName(),
				*Planet->GetPlanetName()), 6.f);
		}
		if (FParse::Param(FCommandLine::Get(), TEXT("PipTest")))
		{
			PipTestRun();
		}
	});
}

FVector ARollerDroid::GetUp() const
{
	return Planet ? Planet->UpAt(Ball->GetComponentLocation()) : FVector::UpVector;
}

bool ARollerDroid::IsGrounded() const
{
	const FVector Start = Ball->GetComponentLocation();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PipGround), false, this);
	return GetWorld()->SweepSingleByChannel(Hit, Start, Start - GetUp() * 10.f, FQuat::Identity,
		ECC_Visibility, FCollisionShape::MakeSphere(BallRadius * 0.9f), Params);
}

void ARollerDroid::ApplySpeedLimit()
{
	const float Speed = bCareful ? CarefulSpeed : MaxSpeed * (bBoost ? BoostMultiplier : 1.f);
	Ball->SetPhysicsMaxAngularVelocityInRadians(Speed / BallRadius);
}

void ARollerDroid::MoveForward(float Value) { MoveInput.X = Value; }
void ARollerDroid::MoveRight(float Value) { MoveInput.Y = Value; }
void ARollerDroid::Turn(float Value) { PendingYaw += Value * MouseDegreesPerUnit; }
void ARollerDroid::LookUp(float Value) { PendingPitch -= Value * MouseDegreesPerUnit; }
void ARollerDroid::StartBoost() { bBoost = true; ApplySpeedLimit(); }
void ARollerDroid::StopBoost() { bBoost = false; ApplySpeedLimit(); }
void ARollerDroid::StartCareful() { bCareful = true; ApplySpeedLimit(); }
void ARollerDroid::StopCareful() { bCareful = false; ApplySpeedLimit(); }

void ARollerDroid::Jump()
{
	if (!bBoarded && !bKnockedOut && IsGrounded())
	{
		Ball->AddImpulse(GetUp() * JumpSpeed, NAME_None, true);
	}
}

void ARollerDroid::ZoomIn() { CameraBoom->TargetArmLength = FMath::Clamp(CameraBoom->TargetArmLength - 60.f, 150.f, 1200.f); }
void ARollerDroid::ZoomOut() { CameraBoom->TargetArmLength = FMath::Clamp(CameraBoom->TargetArmLength + 60.f, 150.f, 1200.f); }

void ARollerDroid::ShowMessage(const FString& Text, float Seconds)
{
	Message = Text;
	MessageStart = Clock;
	MessageUntil = Clock + Seconds;
	UE_LOG(LogTemp, Display, TEXT("[Mission] %s"), *Text);
}

// ----------------------------------------------------------------------------- interaction

AActor* ARollerDroid::FindInteractable() const
{
	if (!Planet || bBoarded || bKnockedOut)
	{
		return nullptr;
	}
	const FVector Here = Ball->GetComponentLocation();
	AActor* Best = nullptr;
	float BestDist = PickupRange;
	for (AShipPart* Part : Planet->GetParts())
	{
		if (IsValid(Part))
		{
			const float D = FVector::Dist(Here, Part->GetActorLocation());
			if (D < BestDist)
			{
				Best = Part;
				BestDist = D;
			}
		}
	}
	if (!Best)
	{
		ASpaceship* Ship = Planet->GetShip();
		if (Ship && !Ship->IsLaunching() && FVector::Dist(Here, Ship->GetActorLocation()) < ShipRange
			&& (Inventory.Num() > 0 || Ship->IsRepaired()))
		{
			Best = Ship;
		}
	}
	return Best;
}

FString ARollerDroid::GetInteractPrompt() const
{
	AActor* Target = FindInteractable();
	if (const AShipPart* Part = Cast<AShipPart>(Target))
	{
		return FString::Printf(TEXT("[E]  Pick up %s"), Mission::Part(Part->GetPartIndex()).Name);
	}
	if (const ASpaceship* Ship = Cast<ASpaceship>(Target))
	{
		return Ship->IsRepaired() ? FString(TEXT("[E]  Launch to the next planet"))
			: FString::Printf(TEXT("[E]  Install %d part%s"), Inventory.Num(), Inventory.Num() == 1 ? TEXT("") : TEXT("s"));
	}
	return FString();
}

void ARollerDroid::Interact()
{
	AActor* Target = FindInteractable();
	if (AShipPart* Part = Cast<AShipPart>(Target))
	{
		const int32 Index = Part->GetPartIndex();
		Inventory.AddUnique(Index);
		Part->Destroy();
		const int32 Found = Inventory.Num() + (Planet->GetShip() ? Planet->GetShip()->NumInstalled() : 0);
		ShowMessage(Found == Mission::NumParts
			? FString::Printf(TEXT("Got the %s. That's all 5! Return to the ship."), Mission::Part(Index).Name)
			: FString::Printf(TEXT("Got the %s (%d/5)"), Mission::Part(Index).Name, Found));
	}
	else if (ASpaceship* Ship = Cast<ASpaceship>(Target))
	{
		if (Ship->IsRepaired())
		{
			ShowMessage(TEXT("Engines online. Lift-off!"), 6.f);
			Ship->Launch(this);
			Board(Ship);
		}
		else
		{
			const int32 N = Ship->InstallParts(Inventory);
			ShowMessage(Ship->IsRepaired() ? FString(TEXT("Ship repaired! Press E to launch."))
				: FString::Printf(TEXT("Installed %d part%s (%d/5). Keep looking!"), N, N == 1 ? TEXT("") : TEXT("s"), Ship->NumInstalled()));
		}
	}
}

void ARollerDroid::Board(AActor* Target)
{
	bBoarded = true;
	WatchTarget = Target;
	MoveInput = FVector2D::ZeroVector;
	Ball->SetSimulatePhysics(false);
	Ball->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetVisibility(false);
	HeadMesh->SetVisibility(false);
	CameraBoom->TargetArmLength = 900.f;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->CameraLagSpeed = 3.f;
}

void ARollerDroid::TeleportNear(const FVector& Target, float Distance)
{
	const FVector Up = Planet ? Planet->UpAt(Target) : FVector::UpVector;
	const FVector Side = Tangent(FVector::CrossProduct(Up, FVector::ForwardVector), Up, FVector::RightVector);
	FVector Point = Target - Side * Distance;
	if (Planet)
	{
		FVector Normal;
		Planet->ProjectToSurface(Point - Planet->GetActorLocation(), Point, Normal);
	}
	const FVector PointUp = Planet ? Planet->UpAt(Point) : FVector::UpVector;
	Ball->SetWorldLocation(Point + PointUp * (BallRadius + 15.f), false, nullptr, ETeleportType::TeleportPhysics);
	Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Ball->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	CamForward = Tangent(Target - Point, PointUp, CamForward);
	PrevVelocity = FVector::ZeroVector;
}

// ----------------------------------------------------------------------------- tick

void ARollerDroid::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;

	if (bBoarded)
	{
		UpdateCamera(DeltaSeconds);
		return;
	}

	if (Clock < AutoDriveUntil)
	{
		MoveInput = AutoDriveInput; // scripted input (PipDrive) overrides the axis bindings
	}
	if (bKnockedOut)
	{
		MoveInput = FVector2D::ZeroVector;
	}

	const FVector Up = GetUp();
	if (Planet)
	{
		Ball->AddForce(-Up * Planet->Gravity, NAME_None, true);
	}

	UpdateCamera(DeltaSeconds);

	// Camera-relative input direction in the local tangent plane.
	const FVector Right = FVector::CrossProduct(Up, CamForward);
	const FVector Dir = (CamForward * MoveInput.X + Right * MoveInput.Y).GetClampedToMaxSize(1.f);
	const bool bGrounded = IsGrounded();
	const FVector V = Ball->GetPhysicsLinearVelocity();
	const FVector VTangent = FVector::VectorPlaneProject(V, Up);

	if (bTestRunning && Clock >= NextTelemetryTime)
	{
		NextTelemetryTime = Clock + 0.5f;
		const float Altitude = Planet ? FVector::Dist(Ball->GetComponentLocation(), Planet->GetActorLocation()) - Planet->Radius
			: GetActorLocation().Z;
		UE_LOG(LogTemp, Display, TEXT("[PipTest] t=%.1f pos=(%.0f,%.0f,%.0f) altitude=%.0f speed=%.0f spin=%.1f rad/s input=(%.1f,%.1f) grounded=%d headUp=%.0f inv=%d hp=%.0f shield=%.0f blaster=%.0f"),
			Clock, GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z, Altitude, VTangent.Size(),
			Ball->GetPhysicsAngularVelocityInRadians().Size(), MoveInput.X, MoveInput.Y, bGrounded ? 1 : 0,
			FVector::DotProduct(HeadMesh->GetComponentLocation() - GetActorLocation(), Up), Inventory.Num(), Health,
			ShieldTime, BlasterTime);
	}

	if (!Dir.IsNearlyZero())
	{
		IdleTime = 0.f;
		if (bGrounded)
		{
			// Rolling along Dir needs angular velocity about Up x Dir.
			const FVector Axis = FVector::CrossProduct(Up, Dir);
			Ball->AddTorqueInRadians(Axis * RollAcceleration * (bBoost ? 1.4f : 1.f), NAME_None, true);
		}
		else
		{
			Ball->AddForce(Dir * RollAcceleration * BallRadius * AirControl, NAME_None, true);
		}
	}
	else
	{
		IdleTime += DeltaSeconds;
		if (bGrounded)
		{
			// Coast down from speed with a gentle brake; once slow, park firmly so gravity on the slope
			// cannot keep the droid creeping downhill.
			const bool bPark = VTangent.Size() < ParkBelowSpeed;
			const float Keep = FMath::Exp(-(bPark ? ParkBrakeRate : BrakeRate) * DeltaSeconds);
			Ball->SetPhysicsAngularVelocityInRadians(Ball->GetPhysicsAngularVelocityInRadians() * Keep);
			if (bPark)
			{
				Ball->SetPhysicsLinearVelocity(VTangent * Keep + Up * FVector::DotProduct(V, Up));
			}
		}
	}

	UpdateHazards(DeltaSeconds);
	UpdatePowerUps(DeltaSeconds);
	UpdateHead(DeltaSeconds);
	UpdateGlow(DeltaSeconds);
}

void ARollerDroid::UpdateCamera(float DeltaSeconds)
{
	const FVector Up = GetUp();
	// parallel-transport the heading into the new tangent plane, then apply mouse yaw about the local up
	CamForward = Tangent(CamForward, Up, GetActorForwardVector());
	CamForward = FQuat(Up, FMath::DegreesToRadians(PendingYaw)).RotateVector(CamForward);
	CamPitch = FMath::Clamp(CamPitch + PendingPitch, -75.f, 20.f);
	PendingYaw = PendingPitch = 0.f;

	const float P = FMath::DegreesToRadians(CamPitch);
	FVector Look = CamForward * FMath::Cos(P) + Up * FMath::Sin(P);
	if (WatchTarget)
	{
		const FVector ToTarget = (WatchTarget->GetActorLocation() + Up * 250.f - Ball->GetComponentLocation()).GetSafeNormal();
		Look = FMath::Lerp(Look, ToTarget, FMath::Clamp(DeltaSeconds * 2.f, 0.f, 1.f)).GetSafeNormal();
		CamForward = Tangent(Look, Up, CamForward);
		// a long arm looking up would put the camera underground: raise the pivot and cap the pitch
		CamPitch = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(FVector::DotProduct(Look, Up), -1.f, 1.f)));
		CamPitch = FMath::Clamp(CamPitch, -20.f, 32.f);
		const float WP = FMath::DegreesToRadians(CamPitch);
		Look = CamForward * FMath::Cos(WP) + Up * FMath::Sin(WP);
	}
	CameraBoom->SetWorldRotation(FRotationMatrix::MakeFromXZ(Look, Up).ToQuat());
	CameraBoom->TargetOffset = Up * (WatchTarget ? 520.f : 45.f);
}

void ARollerDroid::PipDrive(float Seconds, float Forward, float Right)
{
	AutoDriveInput = FVector2D(Forward, Right);
	AutoDriveUntil = Clock + Seconds;
	if (Seconds <= 0.f)
	{
		MoveInput = FVector2D::ZeroVector;
	}
}

void ARollerDroid::PipTestRun()
{
	bTestRunning = true;
	UE_LOG(LogTemp, Display, TEXT("[PipTest] start at %s planet=%s"), *GetActorLocation().ToString(),
		Planet ? *Planet->GetPlanetName() : TEXT("none"));
	FTimerManager& Timers = GetWorldTimerManager();
	auto At = [&Timers](float Delay, TFunction<void()> Fn)
	{
		FTimerHandle Handle;
		Timers.SetTimer(Handle, FTimerDelegate::CreateLambda(MoveTemp(Fn)), Delay, false);
	};
	auto Shot = [this](const TCHAR* Label)
	{
		UE_LOG(LogTemp, Display, TEXT("[PipTest] shot %s"), Label);
		if (APlayerController* PC = Cast<APlayerController>(Controller))
		{
			PC->ConsoleCommand(TEXT("HighResShot 1280x720"));
		}
	};
	auto Quit = [this]
	{
		UE_LOG(LogTemp, Display, TEXT("[PipTest] done"));
		if (APlayerController* PC = Cast<APlayerController>(Controller))
		{
			PC->ConsoleCommand(TEXT("quit"));
		}
	};

	if (!Planet)
	{
		At(3.0f, [Shot] { Shot(TEXT("rest")); });
		At(3.5f, [this] { PipDrive(3.f, 1.f, 0.f); });
		At(5.5f, [Shot] { Shot(TEXT("rolling")); });
		At(6.5f, [this] { PipDrive(2.f, 1.f, 0.6f); StartBoost(); });
		At(7.6f, [this] { Jump(); });
		At(8.5f, [this] { StopBoost(); PipDrive(0.f, 0.f, 0.f); });
		At(9.0f, [Shot] { Shot(TEXT("after_jump")); });
		At(14.0f, [Shot] { Shot(TEXT("curious")); });
		At(15.0f, Quit);
		return;
	}
	if (Planet->GetPlanetNumber() > 1)
	{
		At(4.0f, [Shot] { Shot(TEXT("next_planet")); });
		At(5.0f, Quit);
		return;
	}

	// Planet mission run: drive over the curvature, collect every part, repair and launch.
	At(3.0f, [Shot] { Shot(TEXT("planet_spawn")); });
	At(3.2f, [this] { PendingYaw += 100.f; });                 // turn away from the ship
	At(3.5f, [this] { PipDrive(7.f, 1.f, 0.f); });
	At(7.0f, [this] { StartBoost(); });
	At(9.0f, [this] { Jump(); });
	At(10.5f, [this] { StopBoost(); PipDrive(0.f, 0.f, 0.f); });
	At(11.0f, [Shot] { Shot(TEXT("planet_drive")); });
	// --- hazards and power-ups
	auto First = [this](EHazardKind Kind, int32 Skip = 0) -> TWeakObjectPtr<AHazard>
	{
		for (AHazard* H : Planet->GetHazards())
		{
			if (IsValid(H) && H->GetKind() == Kind && Skip-- <= 0)
			{
				return H;
			}
		}
		return nullptr;
	};
	auto FirstPower = [this](EPowerUpKind Kind) -> TWeakObjectPtr<APowerUp>
	{
		for (APowerUp* P : Planet->GetPowerUps())
		{
			if (IsValid(P) && P->GetKind() == Kind)
			{
				return P;
			}
		}
		return nullptr;
	};
	auto DropOn = [this](const TWeakObjectPtr<AActor>& A)
	{
		if (A.IsValid())
		{
			const FVector Up = Planet->UpAt(A->GetActorLocation());
			Ball->SetWorldLocation(A->GetActorLocation() + Up * 60.f, false, nullptr, ETeleportType::TeleportPhysics);
			Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Ball->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
		}
	};
	auto Log = [this](const TCHAR* What) { UE_LOG(LogTemp, Display, TEXT("[PipTest] %s: hp=%.0f shield=%.0f blaster=%.0f"), What, Health, ShieldTime, BlasterTime); };
	TWeakObjectPtr<AHazard> Cactus = First(EHazardKind::Cactus), Cactus2 = First(EHazardKind::Cactus, 5);
	TWeakObjectPtr<AHazard> Lava = First(EHazardKind::Lava), Thorn = First(EHazardKind::Spikes, 20), Vent = First(EHazardKind::Vent);
	At(12.0f, [this, Cactus] { if (Cactus.IsValid()) { TeleportNear(Cactus->GetActorLocation(), 190.f); CamPitch = -12.f; } });
	At(12.3f, [this] { PipDrive(1.5f, 1.f, 0.f); });
	At(13.0f, [Shot] { Shot(TEXT("cactus_hit")); });
	At(14.0f, [Log] { Log(TEXT("after cactus (expect <100)")); });
	At(14.5f, [DropOn, Lava] { DropOn(Lava); });
	At(15.3f, [Shot] { Shot(TEXT("lava")); });
	At(16.3f, [Log] { Log(TEXT("after lava (expect lower)")); });
	At(16.8f, [DropOn, P = FirstPower(EPowerUpKind::Heal)] { DropOn(P); });
	At(17.3f, [Log] { Log(TEXT("after repair kit (expect +40)")); });
	At(17.5f, [this, Thorn] { if (Thorn.IsValid()) { TeleportNear(Thorn->GetActorLocation(), 260.f); } StartCareful(); });
	At(18.0f, [this] { PipDrive(3.f, 1.f, 0.f); });
	At(19.5f, [Shot] { Shot(TEXT("thorns_careful")); });
	At(21.2f, [this, Log] { StopCareful(); Log(TEXT("after careful thorns (expect unchanged)")); });
	At(21.5f, [DropOn, Vent] { DropOn(Vent); });
	At(29.5f, [Log] { Log(TEXT("after standing on a vent 8 s (expect eruption damage)")); });
	At(30.0f, [DropOn, P = FirstPower(EPowerUpKind::Shield)] { DropOn(P); });
	At(30.5f, [DropOn, Lava] { DropOn(Lava); });
	At(31.4f, [Shot] { Shot(TEXT("shield_lava")); });
	At(32.0f, [Log] { Log(TEXT("shielded lava (expect unchanged)")); });
	At(32.5f, [DropOn, P = FirstPower(EPowerUpKind::Blaster)] { DropOn(P); });
	At(33.0f, [this, Cactus2] { if (Cactus2.IsValid()) { TeleportNear(Cactus2->GetActorLocation(), 500.f); CamPitch = -12.f; } });
	At(33.4f, [this] { PipFire(); });
	At(33.7f, [this] { PipFire(); });
	At(33.75f, [Shot] { Shot(TEXT("blaster")); });
	At(34.0f, [this] { PipFire(); });
	At(34.6f, [Cactus2] { UE_LOG(LogTemp, Display, TEXT("[PipTest] blaster target cactus %s"), Cactus2.IsValid() ? TEXT("STILL STANDING") : TEXT("destroyed")); });
	At(35.0f, [this] { ShieldTime = 0.f; InvulnerableUntil = 0.f; ApplyHazardDamage(999.f, GetActorLocation(), TEXT("Test knockout"), FVector::ZeroVector); });
	At(36.0f, [Shot] { Shot(TEXT("knocked_out")); });
	At(38.5f, [this, Log] { Log(TEXT("after reboot (expect 100, at ship)")); UE_LOG(LogTemp, Display, TEXT("[PipTest] distance to ship %.0f"), FVector::Dist(GetActorLocation(), Planet->GetShip()->GetActorLocation())); });

	At(39.0f, [this, T2 = First(EHazardKind::Spikes, 40)] { if (T2.IsValid()) { TeleportNear(T2->GetActorLocation(), 300.f); } });
	At(39.2f, [this] { StartBoost(); PipDrive(1.8f, 1.f, 0.f); });
	At(40.0f, [Shot] { Shot(TEXT("thorns_fast")); });
	At(41.2f, [this, Log] { StopBoost(); Log(TEXT("after fast thorns (expect damage)")); });

	TArray<TWeakObjectPtr<AShipPart>> Parts;
	for (AShipPart* Part : Planet->GetParts())
	{
		Parts.Add(Part);
	}
	float T = 42.f;
	for (int32 i = 0; i < Parts.Num(); ++i, T += 1.5f)
	{
		At(T, [this, P = Parts[i]] { if (P.IsValid()) { TeleportNear(P->GetActorLocation(), 180.f); InvulnerableUntil = Clock + 3.f; } });
		// pull the camera back for an overview of each part's challenge
		At(T + 0.3f, [this] { CameraBoom->TargetArmLength = 1100.f; CamPitch = -40.f; });
		At(T + 0.9f, [Shot] { Shot(TEXT("challenge")); });
		At(T + 1.2f, [this] { CameraBoom->TargetArmLength = 340.f; CamPitch = -12.f; });
		At(T + 1.0f, [this] { UE_LOG(LogTemp, Display, TEXT("[PipTest] prompt '%s'"), *GetInteractPrompt()); Interact(); });
	}
	At(T, [this] { if (ASpaceship* S = Planet->GetShip()) { TeleportNear(S->GetActorLocation(), 330.f); } });
	At(T + 1.0f, [Shot] { Shot(TEXT("at_ship_inventory")); });
	At(T + 1.5f, [this] { UE_LOG(LogTemp, Display, TEXT("[PipTest] prompt '%s'"), *GetInteractPrompt()); Interact(); });
	At(T + 2.5f, [Shot] { Shot(TEXT("ship_repaired")); });
	At(T + 3.0f, [this] { UE_LOG(LogTemp, Display, TEXT("[PipTest] prompt '%s'"), *GetInteractPrompt()); Interact(); });
	At(T + 6.0f, [Shot] { Shot(TEXT("launch")); });
	// the ship travels to planet 2 after ASpaceship::TravelAfter; that run shoots once and quits
	At(T + 30.0f, Quit); // safety net
	At(13.5f, [this] { UE_LOG(LogTemp, Display, TEXT("[PipTest] challenges: %s / %s / %s / %s / %s"), *Planet->GetChallengeName(0),
		*Planet->GetChallengeName(1), *Planet->GetChallengeName(2), *Planet->GetChallengeName(3), *Planet->GetChallengeName(4)); });
}

void ARollerDroid::UpdateHead(float DeltaSeconds)
{
	const float Dt = FMath::Max(DeltaSeconds, 1e-3f);
	const FVector Up = GetUp();
	const FVector Center = Ball->GetComponentLocation();
	const FVector Velocity = Ball->GetPhysicsLinearVelocity();
	const FVector Flat = FVector::VectorPlaneProject(Velocity, Up);

	const FVector Accel = FVector::VectorPlaneProject((Velocity - PrevVelocity) / Dt, Up);
	PrevVelocity = Velocity;
	SmoothedAccel = FMath::VInterpTo(SmoothedAccel, Accel, Dt, 6.f);

	// Where to look: travel direction when moving; when idle the camera's view, and after a while turn
	// round to look at the camera, with the odd curious glance to the side.
	FVector Target;
	if (Flat.Size() > 40.f)
	{
		Target = Flat.GetSafeNormal();
		GlanceYaw = 0.f;
	}
	else
	{
		const bool bCurious = IdleTime > CuriousAfter;
		if (bCurious && Clock > NextGlanceTime)
		{
			GlanceYaw = FMath::FRandRange(-50.f, 50.f);
			NextGlanceTime = Clock + FMath::FRandRange(1.5f, 4.f);
		}
		Target = bCurious ? FQuat(Up, FMath::DegreesToRadians(180.f + GlanceYaw)).RotateVector(CamForward) : CamForward;
	}
	HeadForward = Tangent(HeadForward, Up, CamForward);
	const float Delta = SignedAngle(HeadForward, Tangent(Target, Up, CamForward), Up);
	HeadForward = FQuat(Up, Delta * FMath::Clamp(Dt * HeadTurnSpeed, 0.f, 1.f)).RotateVector(HeadForward);

	// Lean: back against acceleration (inertia), forward into the direction of travel (counterweight).
	FVector Lean = -SmoothedAccel * LeanPerAccel + Flat * LeanPerSpeed; // degrees, as a tangent vector
	Lean = Lean.GetClampedToMaxSize(MaxLeanDegrees);
	FQuat TargetLean = FQuat::Identity;
	if (!Lean.IsNearlyZero(0.01f))
	{
		const FVector Axis = FVector::CrossProduct(Up, Lean.GetSafeNormal());
		TargetLean = FQuat(Axis, FMath::DegreesToRadians(Lean.Size())); // tilts Up towards Lean
	}
	HeadLean = FQuat::Slerp(HeadLean, TargetLean, FMath::Clamp(Dt * 8.f, 0.f, 1.f));

	const float Bob = IdleTime > 0.5f ? FMath::Sin(Clock * 2.2f) * 0.6f : FMath::Sin(Clock * 14.f) * 0.15f;
	const FVector HeadUp = HeadLean.RotateVector(Up);
	const FVector HeadLocation = Center + HeadUp * (BallRadius + HeadGap + Bob);
	const FQuat HeadRotation = HeadLean * FRotationMatrix::MakeFromXZ(HeadForward, Up).ToQuat();
	HeadMesh->SetWorldLocationAndRotation(HeadLocation, HeadRotation);
}

void ARollerDroid::UpdateGlow(float DeltaSeconds)
{
	if (Clock > NextBlinkTime)
	{
		BlinkUntil = Clock + 0.12f;
		NextBlinkTime = Clock + FMath::FRandRange(2.5f, 6.f);
	}
	const float Blink = Clock < BlinkUntil ? 0.05f : 1.f;
	const float Pulse = 1.f + GlowPulseAmount * FMath::Sin(Clock * GlowPulseSpeed * 2.f * PI);
	const float Boost = bBoost ? 1.6f : 1.f;
	for (int32 i = 0; i < GlowMaterials.Num(); ++i)
	{
		if (GlowMaterials[i])
		{
			GlowMaterials[i]->SetScalarParameterValue(EmissiveParam, GlowBaseStrength[i] * Pulse * Blink * Boost);
		}
	}
}

// ----------------------------------------------------------------------------- health, hazards, power-ups

void ARollerDroid::ApplyHazardDamage(float Amount, const FVector& Source, const FString& Reason, const FVector& VelocityChange)
{
	if (bBoarded || bKnockedOut)
	{
		return;
	}
	if (Clock < InvulnerableUntil)
	{
		return;
	}
	InvulnerableUntil = Clock + 0.7f;
	if (!VelocityChange.IsNearlyZero())
	{
		Ball->AddImpulse(VelocityChange, NAME_None, true);
	}
	if (ShieldTime > 0.f)
	{
		ShowMessage(FString::Printf(TEXT("%s Shield absorbed it."), *Reason), 1.5f);
		UE_LOG(LogTemp, Display, TEXT("[Health] blocked by shield: %s"), *Reason);
		return;
	}
	Health = FMath::Max(0.f, Health - Amount);
	LastDamageTime = Clock;
	UE_LOG(LogTemp, Display, TEXT("[Health] -%.0f (%s) -> %.0f"), Amount, *Reason, Health);
	if (Health <= 0.f)
	{
		KnockOut();
	}
	else
	{
		ShowMessage(FString::Printf(TEXT("%s  -%.0f HP"), *Reason, Amount), 1.6f);
	}
}

void ARollerDroid::KnockOut()
{
	bKnockedOut = true;
	MoveInput = FVector2D::ZeroVector;
	ShowMessage(FString::Printf(TEXT("%s is out of power! Rebooting at the ship..."), DroidName()), 3.f);
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, this, &ARollerDroid::Reboot, 2.5f, false);
}

void ARollerDroid::Reboot()
{
	if (Planet && Planet->GetShip())
	{
		TeleportNear(Planet->GetShip()->GetActorLocation(), 330.f);
	}
	Health = MaxHealth;
	bKnockedOut = false;
	InvulnerableUntil = Clock + 2.f;
	ShowMessage(TEXT("Systems restored. Carried parts are still in the inventory."), 3.f);
	UE_LOG(LogTemp, Display, TEXT("[Health] rebooted at the ship, hp=%.0f"), Health);
}

void ARollerDroid::UpdateHazards(float DeltaSeconds)
{
	ShieldTime = FMath::Max(0.f, ShieldTime - DeltaSeconds);
	BlasterTime = FMath::Max(0.f, BlasterTime - DeltaSeconds);
	const FVector Up = GetUp();
	const FVector Center = Ball->GetComponentLocation();
	if (ShieldBubble)
	{
		const bool bOn = ShieldTime > 0.f;
		ShieldBubble->SetVisibility(bOn);
		if (bOn)
		{
			ShieldBubble->SetWorldLocation(Center + Up * 14.f);
			// pulse, and blink during the last 3 seconds
			const float Blink = ShieldTime < 3.f && FMath::Frac(Clock * 4.f) < 0.5f ? 0.2f : 1.f;
			ShieldMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.15f, 0.4f, 1.f) * (0.35f + 0.1f * FMath::Sin(Clock * 6.f)) * Blink);
		}
	}
	if (!Planet || bBoarded || bKnockedOut)
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PipHazards), true, this);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, Objects, FCollisionShape::MakeSphere(BallRadius + 6.f), Params);
	const FVector V = Ball->GetPhysicsLinearVelocity();
	const FVector VT = FVector::VectorPlaneProject(V, Up);
	bool bInThorns = false;
	for (const FOverlapResult& O : Overlaps)
	{
		AHazard* H = Cast<AHazard>(O.GetActor());
		if (!H || H->IsShattering())
		{
			continue;
		}
		switch (H->GetKind())
		{
		case EHazardKind::Cactus:
		{
			const FVector Away = FVector::VectorPlaneProject(Center - H->GetActorLocation(), Up).GetSafeNormal();
			ApplyHazardDamage(15.f, H->GetActorLocation(), TEXT("Cactus spines!"), Away * 450.f + Up * 250.f);
			break;
		}
		case EHazardKind::Spikes:
			bInThorns = true;
			if (VT.Size() > AHazard::SpikeSafeSpeed)
			{
				ApplyHazardDamage(8.f, H->GetActorLocation(), TEXT("Thorns! Slow down (hold Ctrl)."), Up * 180.f - VT * 0.5f);
			}
			break;
		case EHazardKind::Lava:
			if (Clock >= NextLavaBurn)
			{
				NextLavaBurn = Clock + 0.8f;
				ApplyHazardDamage(12.f, H->GetActorLocation(), TEXT("Lava! Too hot!"), Up * 420.f);
			}
			break;
		default:
			break;
		}
	}
	if (bInThorns)
	{
		// thorns snag: extra drag on the rolling ball
		const float Keep = FMath::Exp(-2.5f * DeltaSeconds);
		Ball->SetPhysicsLinearVelocity(VT * Keep + Up * FVector::DotProduct(V, Up));
	}
}

void ARollerDroid::UpdatePowerUps(float DeltaSeconds)
{
	if (!Planet || bBoarded || bKnockedOut)
	{
		return;
	}
	const FVector Center = Ball->GetComponentLocation();
	for (APowerUp* P : Planet->GetPowerUps())
	{
		if (!IsValid(P) || FVector::Dist(Center, P->GetActorLocation()) > 130.f)
		{
			continue;
		}
		switch (P->GetKind())
		{
		case EPowerUpKind::Heal:
			Health = FMath::Min(MaxHealth, Health + 40.f);
			ShowMessage(TEXT("Repair kit: +40 HP"), 2.f);
			break;
		case EPowerUpKind::Shield:
			ShieldTime = ShieldDuration;
			ShowMessage(FString::Printf(TEXT("Shield up for %.0f s"), ShieldDuration), 2.f);
			break;
		case EPowerUpKind::Blaster:
			BlasterTime = BlasterDuration;
			ShowMessage(FString::Printf(TEXT("Blaster online for %.0f s: left click shoots cacti, thorns and walls"), BlasterDuration), 3.f);
			break;
		}
		UE_LOG(LogTemp, Display, TEXT("[PowerUp] %s"), APowerUp::KindName(P->GetKind()));
		P->Destroy();
	}
}

FVector ARollerDroid::GetAimDirection() const
{
	const FVector Up = GetUp();
	// the camera looks down at the droid; shoot a little above its view so shots fly level-ish
	const float P = FMath::DegreesToRadians(FMath::Clamp(CamPitch + 12.f, -15.f, 30.f));
	return (CamForward * FMath::Cos(P) + Up * FMath::Sin(P)).GetSafeNormal();
}

FVector ARollerDroid::GetMuzzleLocation() const
{
	return HeadMesh->GetComponentLocation() + GetUp() * 12.f + GetAimDirection() * 30.f;
}

void ARollerDroid::PipFire()
{
	if (BlasterTime <= 0.f || bBoarded || bKnockedOut || Clock < NextFireTime || !Planet)
	{
		return;
	}
	NextFireTime = Clock + 0.22f;
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ABlasterBolt* Bolt = GetWorld()->SpawnActor<ABlasterBolt>(GetMuzzleLocation(), FRotator::ZeroRotator, Params))
	{
		Bolt->Launch(GetAimDirection(), this, Planet->EffectSphereMesh, Planet->BeaconMaterial);
	}
}
