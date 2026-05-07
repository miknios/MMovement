# MMovement

A powerful Unreal Engine plugin for custom character movement. MMovement extends the standard `CharacterMovementComponent` with a flexible, extensible framework for creating and managing custom movement modes, enabling advanced character locomotion systems typically found in hack-and-slash and action games.

## Core Concept

MMovement is built around the idea of **Movement Modes** - discrete, composable movement states that can be easily switched between. Unlike Unreal's default movement system which has a fixed set of states (Walking, Falling, Flying, etc.), MMovement provides a streamlined framework where you can:

- Create an unlimited number of custom movement modes
- Switch between modes dynamically based on game conditions
- Manage complex movement logic (dashing, wall running, sliding) without modifying engine code
- Control character physics during special movement states with precision

The plugin is designed for game designers and programmers to rapidly prototype and iterate on movement mechanics in a modular way.

## Key Features

### 1. Custom Movement Mode Framework
- **`UMMovementMode_Base`**: Blueprintable base class for creating custom movement modes
- **Lifecycle hooks**: Initialize, Tick, CanStart, Start, Phys (movement logic), End
- **State management**: Each mode knows when it can be active and handles its own logic
- **Event delegation**: Built-in delegates (OnStart, OnUpdate, OnEnd) for reacting to mode changes
- **Fully Blueprintable**: Create movement modes entirely in Blueprint with zero C++ coding

### 2. Built-in Movement Modes
- **Wall Run**: Horizontal wall running with speed preservation and momentum-based jumping
- **Vertical Wall Run**: Climb vertical surfaces with configurable speed thresholds
- **Dash**: Quick directional movement with charge system, cooldowns, and optional damage on impact
- **Slide**: Sliding mechanics with slope detection, surface snapping, and momentum-based jumping
- **Forward Movement from Animation Curve**: Drive character movement using animation curves for frame-perfect timing

### 3. Controlled Launch System
**Fine-grained control over launched characters** - for all of your needs where you want to launch a character with precise control over movement falloff:
- **Independent launches**: Multiple launches can be active simultaneously from different sources
- **Physics influence**: Control how much input acceleration, braking deceleration, and gravity affect launched characters
- **Curve-based modulation**: Use animation curves to smoothly transition physics influence over launch duration
- **Landing control**: Block walking state immediately after launch, keeping character in falling state with controlled physics
- **Launch immunity**: Prevent character from being launched. Useful for invulnerability frames or special states
- **Per-owner tracking**: Supports both anonymous launches and owner-tracked launches

### 4. Walking Speed System
- **Speed type assets**: Create multiple speed configurations (Walk, Run, Sprint) as data assets
- **Per-character speed configs**: Each character can have different speeds for each speed type
- **Comprehensive parameters**: Control speed, acceleration, braking deceleration, friction, and more
- **Dynamic switching**: Change speed types at runtime based on game conditions

### 5. Advanced Movement Features
- **Temporal velocity tracking**: Track peak horizontal velocity over multiple frames for smooth state transitions
- **Custom movement bases**: Characters can move on moving platforms or other dynamic bases
- **Root motion integration**: Full support for animation-driven movement
- **Input tracking**: Always know the last movement input direction and the last non-zero input direction
- **Direction utilities**: Helper functions to align movement with floor or specific directions
- **Surface queries**: Determine if character is moving on ground/surface without being in ground movement mode

### 6. Developer Experience
- **Visual Logging**: Full integration with Unreal's Visual Logger for debugging movement behavior
- **Debug visualization**: Draw debug information directly in the viewport
- **State introspection**: Query current movement mode, check if custom modes are active, etc.
- **Enhanced Input support**: Seamless integration with Unreal's Enhanced Input System

## Installation

1. Copy the `MMovement` folder to your project's `Plugins` directory
2. Regenerate Visual Studio project files
3. Build the plugin

### Dependencies

- **MUtility**: Required utility plugin
- **EnhancedInput**: Unreal Engine's Enhanced Input System

## Quick Start

### 1. Set Up Your Character Class

```cpp
#include "MCharacterMovementComponent.h"

AMyCharacter::AMyCharacter()
{
    // Use the custom movement component instead of default CharacterMovementComponent
    CharacterMovement = CreateDefaultSubobject<UMCharacterMovementComponent>(TEXT("CharMoveComp"));
}

void AMyCharacter::BeginPlay()
{
    Super::BeginPlay();
    
    // Ensure movement modes are initialized
    if (UMCharacterMovementComponent* MoveComp = Cast<UMCharacterMovementComponent>(CharacterMovement))
    {
        MoveComp->EnsureMovementModesInitialized();
    }
}
```

### 2. Create a Custom Movement Mode

Extend `UMMovementMode_Base` to create your own movement type:

```cpp
#include "MMovementMode_Base.h"
#include "MCharacterMovementComponent.h"

UCLASS()
class YOURPROJECT_API UMyCustomMovement : public UMMovementMode_Base
{
    GENERATED_BODY()

public:
    // Called when movement mode is initialized
    virtual void Initialize_Implementation() override;
    
    // Check if this mode can be activated
    virtual bool CanStart_Implementation(FString& OutFailReason) override;
    
    // Called when mode becomes active
    virtual void Start_Implementation() override;
    
    // Main movement logic - called every frame while active
    virtual void Phys_Implementation(float DeltaTime, int32 Iterations) override;
    
    // Called when mode is deactivated
    virtual void End_Implementation() override;

protected:
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float MovementSpeed = 500.0f;
};
```

### 3. Add Movement Modes to Your Character

Add the movement modes you want to use to the "Available Movement Modes" array in the movement component details:

In your Character Blueprint or C++ constructor:

```cpp
AMyCharacter::AMyCharacter()
{
    CharacterMovement = CreateDefaultSubobject<UMCharacterMovementComponent>(TEXT("CharMoveComp"));
    
    // Add built-in movement modes
    UMCharacterMovementComponent* MoveComp = Cast<UMCharacterMovementComponent>(CharacterMovement);
    if (MoveComp)
    {
        MoveComp->AvailableMovementModes.Add(UMMovementMode_Dash::StaticClass());
        MoveComp->AvailableMovementModes.Add(UMMovementMode_WallRun::StaticClass());
        MoveComp->AvailableMovementModes.Add(UMMovementMode_Slide::StaticClass());
    }
}
```

Or simply add them in the Character Blueprint Details panel under "Movement | Movement Modes" → "Available Movement Modes" array.

### How Movement Modes Activate

Movement modes are **activated automatically** based on game conditions. Each mode has a `CanStart()` function that determines if it can be activated. When `CanStart()` returns true, the mode becomes active:

```cpp
// In your custom movement mode:
virtual bool CanStart_Implementation(FString& OutFailReason) override
{
    // Check if conditions are met for this mode
    if (!HasRequiredItemEquipped())
    {
        OutFailReason = "Missing required item";
        return false;
    }
    
    // If all conditions pass, mode will activate
    return true;
}
```

Your game logic should handle **when** to attempt activation by calling the appropriate functions or responding to input events that the movement mode listens to (e.g., Enhanced Input).

## Built-in Movement Modes

### Wall Run
Allows characters to run horizontally along walls. This is a horizontal wall-running mechanic where:
- Characters stick to walls and run parallel to them
- Minimum horizontal and vertical speed requirements for starting and continuing
- Speed preservation option: optionally maintain incoming speed or cap to max speed
- Jump off mechanics: Calculate jump velocity based on preserved speed or fixed values
- Cooldown system: Prevents spam and allows for intentional platforming sequencing
- Integration with movement base system for accurate surface tracking

**Key Settings**:
- `MinHorizontalSpeedToStart`: Minimum speed to initiate wall run
- `MaxSpeedFromAcceleration`: Maximum speed achievable through acceleration
- `bEnableSpeedPreservation`: Whether to preserve incoming momentum
- `CooldownTime`: Time before wall run can be used again

### Vertical Wall Run
A specialized wall run for climbing vertical surfaces:
- Climb up or down vertical walls
- Similar speed and cooldown mechanics to horizontal wall running
- Useful for vertical climbing sections and height-based platforming

### Dash
Quick dash movement with momentum preservation and optional charging:
- **Distance-based**: Travel a set distance over a set duration using animation curves
- **Input direction**: Dash in movement input direction or use free aim
- **Vertical control**: Choose whether to include vertical component in dash direction
- **Charge system**: Accumulate charges with customizable max amount and restoration conditions
- **Charge restoration**: Restore on ground, wall run, or vertical wall run entry
- **Damage on hit**: Optional dash strike system that damages enemies along the dash path
- **Velocity preservation**: Preserve some incoming velocity to chain movements
- **Controlled launch on finish**: Optional physics-controlled launch after dash completes

**Key Settings**:
- `Distance`: Total distance traveled during dash
- `Duration`: How long dash lasts (frames are distributed via curve)
- `ChargeAmountMax`: Maximum charge count
- `bEnableDashCharges`: Enable charge system
- `bEnableDamage`: Enable damage on dash hit
- `DashConfig|Controlled Launch`: Apply physics-controlled launch after dash

### Slide
Advanced sliding mechanics with slope detection and momentum-based gameplay:
- **Slope detection**: Automatically detect slopes and apply different physics
- **Even surface sliding**: Smooth sliding on flat ground with deceleration
- **Downslope acceleration**: Speed up when sliding downhill up to max speed cap
- **Upslope deceleration**: Slow down more aggressively on uphill slides
- **Surface snapping**: Smooth snapping to surfaces during slide
- **Cooldown system**: Prevent immediate re-sliding after ending
- **Slope angle filtering**: Only detect surfaces within angle threshold
- **Surface detection tags**: Include/exclude surfaces by tag
- **Player direction control**: Slide in aim direction or movement input direction
- **Jump off mechanics**: Jump out of slide with configurable velocity calculation
- **Controlled launch on jump**: Optional physics-controlled launch when exiting slide via jump

**Key Settings**:
- `SlideSpeedInitial`: Minimum speed applied on slide start
- `SlideEndSpeedThreshold`: Speed below which slide automatically ends
- `DecelerationEvenSurface`: Deceleration on flat ground
- `AccelerationDownwardSlope`: Acceleration bonus on downslopes
- `SlopeNormalAngleMin`: Minimum angle to detect as slope
- `SlideSurfaceDetectionExclusionTags`: Surfaces to ignore for sliding

### Forward Movement from Animation Curve
Drive character forward motion using animation curves for perfect synchronization:
- Animation-driven movement entirely controlled by curve values
- Useful for melee attacks, animation-locked movements, or cutscenes
- Precise timing of movement to match animation frames

## Key Classes

| Class | Purpose |
|-------|---------|
| `UMCharacterMovementComponent` | Extended character movement component with custom mode support and all core features |
| `UMMovementMode_Base` | Base class for creating custom movement modes - fully blueprintable |
| `UMControlledLaunchManager` | Manages active launches and combines their physics effects |
| `UMCharacterMovementWalkingSpeedTypeAsset` | Data asset that represents a movement speed type (Walk, Run, Sprint, etc.) |
| `UMCharacterMovementWalkingSpeedConfig` | Configuration struct for speed, acceleration, friction, and deceleration |
| `UMControlledLaunchAsset` | Data asset for storing reusable launch parameter configurations |

## Movement Mode Lifecycle

Understanding the lifecycle of a movement mode is key to using MMovement effectively. Movement modes **activate automatically** when their conditions are met:

```
1. INITIALIZATION (happens once when character spawns)
   ├─ Initialize() - Set up mode-specific data structures
   └─ Tick() called every frame (even if mode isn't active)

2. CONTINUOUS CHECKING (every frame for inactive modes)
   ├─ Tick() - Allows mode to track input or state changes
   └─ CanStart() - Framework automatically checks if mode can activate
   
3. TRANSITION (when CanStart() returns true)
   ├─ Framework detects conditions are met
   └─ Mode transitions to active state

4. ACTIVE (mode is running)
   ├─ Start() - Called once when mode becomes active
   ├─ Tick() continues
   ├─ Phys() - Apply movement every frame
   └─ [Usually ends when some condition is met or CanStart returns false]

5. ENDING (when deactivating)
   ├─ End() - Clean up this activation
   └─ Tick() continues in background (for next activation check)
```

This lifecycle allows modes to:
- Listen for input or track game state in Tick() even when inactive
- Automatically activate when conditions are met
- Access the character and movement component anytime
- Accumulate state across frames
- Perform validation before activation
- Clean up properly when exiting
- Deactivate automatically when conditions are no longer met


## Advanced Usage

### Understanding the Controlled Launch System

The Controlled Launch System is designed to handle character physics during special events like knockback, skill effects, or momentum-based abilities. Unlike a simple velocity add, it gives you precise control over how physics affects the launched character:

```cpp
void AMyCharacter::ApplySkillKnockback()
{
    if (UMCharacterMovementComponent* MoveComp = Cast<UMCharacterMovementComponent>(CharacterMovement))
    {
        FVector LaunchDirection = GetActorForwardVector();
        FVector LaunchVelocity = LaunchDirection * 2000; // 2000 UU/s
        
        FMControlledLaunchParams LaunchParams;
        LaunchParams.Duration = 1.0f; // Launch effect lasts 1 second
        LaunchParams.WalkingBlockDuration = 0.5f; // Keep character in air for 0.5s
        LaunchParams.bInfluenceInputAcceleration = true; // Let player influence trajectory
        LaunchParams.bInfluenceGravity = false; // Normal gravity applies
        LaunchParams.bDisableOnSurface = true; // End launch if character hits ground
        
        MoveComp->AddControlledLaunchFromParams(LaunchVelocity, LaunchParams, this);
    }
}

void AMyCharacter::OnEnemyAttack(AActor* AttackingEnemy)
{
    if (UMCharacterMovementComponent* MoveComp = Cast<UMCharacterMovementComponent>(CharacterMovement))
    {
        FVector KnockbackDirection = (GetActorLocation() - AttackingEnemy->GetActorLocation()).GetSafeNormal();
        FVector KnockbackVelocity = KnockbackDirection * 1500 + FVector(0, 0, 500); // Forward + upward
        
        FMControlledLaunchParams KnockbackParams;
        KnockbackParams.Duration = 0.8f;
        KnockbackParams.WalkingBlockDuration = 1.0f;
        KnockbackParams.bInfluenceInputAcceleration = true; // Player can steer mid-knockback
        KnockbackParams.bInfluenceGravity = true;
        
        MoveComp->AddControlledLaunchFromParams(KnockbackVelocity, KnockbackParams, this);
    }
}
```

**Key Concepts**:
- **Duration**: How long the launch effect manages character physics
- **WalkingBlockDuration**: After landing, temporarily prevent walking (keep in falling state with controlled physics)
- **bInfluenceInputAcceleration**: Whether player input can affect launched character
- **bInfluenceGravity**: Whether gravity affects launched character during launch
- **AccelerationMultiplierCurve**: Animate how much input affects the character over time (0-1 curve)
- **BrakingDecelerationMultiplierCurve**: Animate how much braking affects the character
- **bDisableOnSurface**: Auto-cancel launch if character hits a surface (ground, wall, etc.)

### Using Launch Assets for Reusable Launch Configurations

Create launch assets that define launch parameters you can reuse across multiple abilities:

```cpp
// In your asset editor or C++:
// Create a Data Asset of type UMControlledLaunchAsset with these settings:
// - Duration: 0.5f
// - bInfluenceGravity: true with gravity multiplier curve
// - bInfluenceInputAcceleration: true with acceleration curve
// - Then use it in multiple places:

void AMyCharacter::ApplyAbilityKnockback(UMControlledLaunchAsset* LaunchAsset)
{
    if (UMCharacterMovementComponent* MoveComp = Cast<UMCharacterMovementComponent>(CharacterMovement))
    {
        FVector LaunchVelocity = GetActorUpVector() * 1500;
        MoveComp->AddControlledLaunchFromAsset(LaunchVelocity, LaunchAsset, this);
    }
}
```

### Working with Multiple Simultaneous Launches

The system supports multiple launches from different sources and combines their effects:

```cpp
void AMyCharacter::ActivateSpecialAbility()
{
    if (UMCharacterMovementComponent* MoveComp = Cast<UMCharacterMovementComponent>(CharacterMovement))
    {
        // Launch 1: Upward boost
        FVector UpLaunch = FVector(0, 0, 1500);
        FMControlledLaunchParams UpParams;
        UpParams.Duration = 0.5f;
        UpParams.bInfluenceGravity = true;
        MoveComp->AddControlledLaunchFromParams(UpLaunch, UpParams, this);
        
        // Launch 2: Forward momentum (can be active simultaneously!)
        FVector ForwardLaunch = GetActorForwardVector() * 2000;
        FMControlledLaunchParams ForwardParams;
        ForwardParams.Duration = 1.0f;
        MoveComp->AddControlledLaunchFromParams(ForwardLaunch, ForwardParams, this);
        
        // Both launches act on the character, their effects combine
    }
}
```

### Customize Walking Speed

Use the Walking Speed System to support multiple movement speeds:

```cpp
// Create speed type assets in the editor (Walk, Run, Sprint)
// Then configure them per-character:

void AMyCharacter::SetupMovementSpeeds()
{
    if (UMCharacterMovementComponent* MoveComp = Cast<UMCharacterMovementComponent>(CharacterMovement))
    {
        // Create configuration for "Sprint" speed type
        FMCharacterMovementWalkingSpeedConfig SprintConfig;
        SprintConfig.Speed = 1500; // Units per second
        SprintConfig.Acceleration = 4096;
        SprintConfig.BrakingDeceleration = 4096;
        SprintConfig.GroundFriction = 8;
        
        // Apply to character
        MoveComp->SetWalkingSpeedConfig(SprintSpeedType, SprintConfig);
        
        // Later, switch to sprint
        MoveComp->SetWalkingSpeedType(SprintSpeedType);
    }
}
```

### Querying Movement State

Check current movement state to make decisions in your code:

```cpp
void AMyCharacter::HandleMovement()
{
    if (UMCharacterMovementComponent* MoveComp = Cast<UMCharacterMovementComponent>(CharacterMovement))
    {
        // Is any custom movement mode active?
        if (MoveComp->IsCurrentMovementModeCustom())
        {
            // Which mode exactly?
            UMMovementMode_Base* ActiveMode = MoveComp->GetActiveCustomMovementModeInstance();
            if (ActiveMode && ActiveMode->GetMovementModeName() == TEXT("WallRun"))
            {
                // Special logic for wall running
            }
        }
        
        // Are we affected by a controlled launch?
        UMControlledLaunchManager* LaunchMgr = MoveComp->GetControlledLaunchManager();
        if (LaunchMgr->IsAnyControlledLaunchActive())
        {
            // Maybe disable certain input during launch
        }
        
        // Get current velocity info
        FVector HorizontalVelocity = MoveComp->GetHorizontalVelocity();
        FVector PeakTemporalVelocity = MoveComp->GetPeakTemporalHorizontalVelocity();
    }
}
```

### Creating Custom Movement Modes via Blueprints

You can create movement modes entirely in Blueprint without writing C++:

```
1. In Content Browser: Create Blueprint > Search for "Movement Mode Base" > Select it
2. You now have a Blueprint based on UMMovementMode_Base
3. Override these events:
   - Event Initialize: Set up initial state
   - Event CanStart: Return true if mode can be activated
   - Event Start: Called when mode becomes active
   - Event Phys: Your main movement logic here
   - Event End: Clean up when mode ends
   - Event Tick: Called every frame (even when not active)
4. Get references to the character and movement component via:
   - Character Owner
   - Movement Component
5. Use the Helper functions to apply forces, check state, etc.
```

## Blueprint Support

All core functionality is exposed to Blueprints, enabling:
- **Full movement mode creation** in Blueprint without C++ code
- **Dynamic mode switching** via Blueprint nodes
- **Event-driven logic** with OnJumped, OnMovementModeStart, OnMovementModeEnd
- **Parameter configuration** entirely in the editor
- **Real-time debugging** with movement state queries

### Setting up in Blueprint

1. **Create your character class** (C++ or Blueprint based on `Character`)
2. **Replace the movement component** with `MCharacterMovementComponent`:
   - Select Character Blueprint
   - In Details, find "Character Movement"
   - Change Component Class to `MCharacterMovementComponent`
3. **Add movement modes** to the "Available Movement Modes" array:
   - Create Blueprint classes extending `MMovementMode_Base`
   - Add them to the array in the movement component
4. **Implement mode logic** by overriding Blueprint NativeEvents in your mode BP

## Common Patterns

### Pattern 1: Momentum-Based Platforming
Chain Wall Run → Dash → Slide combos by preserving momentum:
- Wall run preserves incoming speed
- Double tap dash to launch off wall → dash in air
- Land and immediately slide to continue momentum
- Use peak temporal velocity to detect good chaining moments

### Pattern 2: Skill-Based Knockback
Apply physics-controlled knockback from abilities:
- Use Controlled Launch System for knockback
- Configure gravity and input influence via curves
- Optional "landing block" prevents immediate walking
- Multiple simultaneous launches from different sources

### Pattern 3: Animation-Locked Melee Attacks
Drive melee attacks with animation curves:
- Use Forward Movement from Animation Curve mode for attack movement
- Dash with animation-driven forward motion
- Slide as a recovery option after attack


## Performance Considerations

- Movement modes are initialized once during BeginPlay
- Phys() is called only for active modes (efficient switching)
- Tick() is called for all modes every frame (consider lightweight checks)
- Temporal velocity history is limited to N frames (default: 3) to save memory

