function Gate() {}

Gate.prototype.Schema =
	"<a:help>Controls behavior of wall gates</a:help>" +
	"<a:example>" +
		"<PassRange>20</PassRange>" +
	"</a:example>" +
	"<element name='PassRange' a:help='Units must be within this distance (in meters) of the gate for it to open'>" +
		"<ref name='nonNegativeDecimal'/>" +
	"</element>";

/**
 * Initialize Gate component
 */
Gate.prototype.Init = function()
{
	this.units = [];
	this.opened = false;
	this.locked = false;
};

Gate.prototype.OnOwnershipChanged = function(msg)
{
	if (msg.to != -1)
	{
		this.SetupRangeQuery(msg.to);
		// Set the initial state, but don't play unlocking sound
		if (!this.locked)
			this.UnlockGate(true);
	}
};

/**
 * Cleanup on destroy
 */
Gate.prototype.OnDestroy = function()
{
	// Clean up range query
	var cmpRangeManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager);
	if (this.unitsQuery)
		cmpRangeManager.DestroyActiveQuery(this.unitsQuery);
};

/**
 * Setup the range query to detect units coming in & out of range
 */
Gate.prototype.SetupRangeQuery = function(owner)
{
	var cmpRangeManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager);
	var cmpPlayerManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_PlayerManager);
	if (this.unitsQuery)
		cmpRangeManager.DestroyActiveQuery(this.unitsQuery);
	var players = []

	var numPlayers = cmpPlayerManager.GetNumPlayers();

	// Ignore gaia units
	for (var i = 1; i < numPlayers; ++i)
		players.push(i);

	var range = this.GetPassRange();
	if (range > 0)
	{
		// Only find entities with IID_UnitAI interface
		this.unitsQuery = cmpRangeManager.CreateActiveQuery(this.entity, 0, range, players, IID_UnitAI, cmpRangeManager.GetEntityFlagMask("normal"));
		cmpRangeManager.EnableActiveQuery(this.unitsQuery);
	}
};

/**
 * Called when units enter or leave range
 */
Gate.prototype.OnRangeUpdate = function(msg)
{
	if (msg.tag != this.unitsQuery)
		return;

	if (msg.added.length > 0)
		for each (var entity in msg.added)
			this.units.push(entity);

	if (msg.removed.length > 0)
		for each (var entity in msg.removed)
			this.units.splice(this.units.indexOf(entity), 1);

	this.OperateGate();
};

/**
 * Get the range in which units are detected
 */
Gate.prototype.GetPassRange = function()
{
	return +this.template.PassRange;
};

/**
 * Attempt to open or close the gate.
 * An ally unit must be in range to open the gate, but there must be
 *	no units (including enemies) in range to close it again.
 */
Gate.prototype.OperateGate = function()
{
	if (this.opened == true )
	{
		// If no units are in range, close the gate
		if (this.units.length == 0)
			this.CloseGate();
	}
	else
	{
		// If one units in range is owned by an ally, open the gate
		for each (var ent in this.units)
		{
			if (IsOwnedByAllyOfEntity(this.entity, ent))
			{
				this.OpenGate();
				break;
			}
		}
	}
};

Gate.prototype.IsLocked = function()
{
	return this.locked;
};

/**
 * Lock the gate, with sound. It will close at the next opportunity.
 */
Gate.prototype.LockGate = function()
{
	this.locked = true;
	// If the door is closed, enable 'block pathfinding'
	// Else 'block pathfinding' will be enabled the next time the gate close
	if (!this.opened)
	{
		var cmpObstruction = Engine.QueryInterface(this.entity, IID_Obstruction);
		if (!cmpObstruction)
			return;
		cmpObstruction.SetDisableBlockMovementPathfinding(false, false, 0);
	}

	// TODO: Possibly move the lock/unlock sounds to UI? Needs testing
	PlaySound("gate_locked", this.entity);
};

/**
 * Unlock the gate, with sound. May open the gate if allied units are within range.
 * If quiet is true, no sound will be played (used for initial setup).
 */
Gate.prototype.UnlockGate = function(quiet)
{
	var cmpObstruction = Engine.QueryInterface(this.entity, IID_Obstruction);
	if (!cmpObstruction)
		return;

	// Disable 'block pathfinding'
	cmpObstruction.SetDisableBlockMovementPathfinding(false, true, 0);
	this.locked = false;

	// TODO: Possibly move the lock/unlock sounds to UI? Needs testing
	if (!quiet)
		PlaySound("gate_unlocked", this.entity);

	// If the gate is closed, open it if necessary
	if (!this.opened)
		this.OperateGate();
};

/**
 * Open the gate if unlocked, with sound and animation.
 */
Gate.prototype.OpenGate = function()
{
	// Do not open the gate if it has been locked
	if (this.locked)
		return;

	var cmpObstruction = Engine.QueryInterface(this.entity, IID_Obstruction);
	if (!cmpObstruction)
		return;

	// Disable 'block movement'
	cmpObstruction.SetDisableBlockMovementPathfinding(true, true, 0);
	this.opened = true;

	PlaySound("gate_opening", this.entity);
	var cmpVisual = Engine.QueryInterface(this.entity, IID_Visual);
	if (cmpVisual)
		cmpVisual.SelectAnimation("gate_opening", true, 1.0, "");
};

/**
 * Close the gate, with sound and animation.
 */
Gate.prototype.CloseGate = function()
{
	var cmpObstruction = Engine.QueryInterface(this.entity, IID_Obstruction);
	if (!cmpObstruction)
		return;

	// If we ordered the gate to be locked, enable 'block movement' and 'block pathfinding'
	if (this.locked)
		cmpObstruction.SetDisableBlockMovementPathfinding(false, false, 0);
	// Else just enable 'block movement'
	else
		cmpObstruction.SetDisableBlockMovementPathfinding(false, true, 0);
	this.opened = false;

	PlaySound("gate_closing", this.entity);
	var cmpVisual = Engine.QueryInterface(this.entity, IID_Visual);
	if (cmpVisual)
		cmpVisual.SelectAnimation("gate_closing", true, 1.0, "");
};

Engine.RegisterComponentType(IID_Gate, "Gate", Gate);
