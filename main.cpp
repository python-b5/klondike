/* klondike
 * by python-b5
 *
 * A simple implementation of Klondike Solitaire (commonly shortened to
 * "Klondike", and often just called "Solitaire" to to its ubiquity), written
 * with C++ and SDL2. Partially a clone of Microsoft Solitaire (from Windows XP
 * and below).
 */


// project includes
#include "wrapper.hpp"

// standard libraries
#include <queue>
#include <stack>
#include <vector>
#include <algorithm>
#include <functional>

// C standard libraries
#include <ctime>
#include <cstdlib>


// declare card sprites
wrapper::Sprite cards[52];
wrapper::Sprite cards_back;
wrapper::Sprite cards_base;


/* A card suit. */
enum Suit {
    CLUBS,
    DIAMONDS,
    HEARTS,
    SPADES
};

/* The type of card being dragged, if any.
 * (this enum was a terrible mistake and is responsible for numerous ugly switch
 * statements across this file) */
enum DragType {
    NONE,
    TOP_CARD,
    FOUNDATION_1, FOUNDATION_2, FOUNDATION_3, FOUNDATION_4,
    TABLEAU_1, TABLEAU_2, TABLEAU_3, TABLEAU_4, TABLEAU_5, TABLEAU_6, TABLEAU_7
};


/* A playing card.
 * Order (from 0): Ace, 2-10, Jack, Queen, King (same as in-game)
 */
struct Card {
    int index;
    Suit suit;

    Card(int index_, Suit suit_):
        index(index_),
        suit(suit_)
    {}
};


/* A dealt playing card for displaying on-screen. */
struct DealtCard {
    Card card;
    bool face_up;

    DealtCard(const Card &card_, bool face_up_):
        card(card_),
        face_up(face_up_)
    {}

    /* Draws the appropriate card sprite at a given position. */
    void draw(int x, int y) const {
        if (face_up) {
            cards[13 * card.suit + card.index].draw(x, y);
        } else {
            cards_back.draw(x, y);
        }
    }
};


/* A stack of dealt cards.
 * This isn't *actually* a stack, since it needs to be iterated through to be
 * drawn.
 */
struct CardStack {
    std::vector<DealtCard> cards;

    CardStack() {}

    CardStack(std::vector<DealtCard> cards_):
        cards(cards_)
    {
        // move face-up cards to the top of the stack
        size_t size = cards.size();

        for (size_t i = 0; i < size; ++i) {
            if (cards[i].face_up) {
                std::vector<DealtCard>::iterator iter = cards.begin() + i;
                std::rotate(iter, iter + 1, cards.end());

                --i;
                --size;
            }
        }
    }

    /* Draws the CardStack at a given position, optionally skipping a provided
     * number of cards (counting from the top of the stack).
     *
     * Face-up cards are drawn with more spacing.
     */
    void draw(int x, int y, int skip = 0) {
        int draw_y = y;

        for (size_t i = 0; i < cards.size() - skip; ++i) {
            cards[i].draw(x, draw_y);

            if (cards[i].face_up) {
                draw_y += 15;
            } else {
                draw_y += 3;
            }
        }
    }

    /* Returns a bounding box representing the top card in the CardStack,
     * starting at a specified position. If the stack is empty, it behaves as if
     * it has one card.
     */
    wrapper::BBox get_top_card_bbox(int start_x, int start_y) {
        wrapper::BBox bbox;

        bbox.x1 = start_x;
        bbox.y1 = start_y;
        bbox.x2 = start_x + 70;
        bbox.y2 = start_y + 95;

        if (cards.empty()) {
            return bbox;
        }

        for (size_t i = 0; i < cards.size(); ++i) {
            if (i != cards.size() - 1) {
                if (cards[i].face_up) {
                    bbox.y1 += 15;
                    bbox.y2 += 15;
                } else {
                    bbox.y1 += 3;
                    bbox.y2 += 3;
                }
            }
        }

        return bbox;
    }
};


/* One of the four foundations. */
struct Foundation {
    int next = 0;
    Suit suit;

    /* Adds a card to the Foundation if it is the correct next card.
     * Returns whether the move was correct.
     */
    bool add(const Card &card) {
        if ((next == 0 || card.suit == suit) && card.index == next) {
            if (next == 0) {
                suit = card.suit;
            }

            ++next;

            return true;
        }

        return false;
    }

    /* Draws the Foundation at a given position, optionally showing the
     * second-to-top card instead of the top one.
     */
    void draw(int x, int y, bool second_to_top) {
        if (next == 0) {
            cards_base.draw(x, y);
        } else {
            if (second_to_top && next == 1) {
                cards_base.draw(x, y);
            } else {
                DealtCard(
                    Card(next - 1 - static_cast<int>(second_to_top), suit),
                    true
                ).draw(x, y);
            }
        }
    }
};


/* Finds the index closest to a given X position, using a provided function to
 * calculate the distance.
 */
size_t closest_index(
    int x, std::vector<size_t> indexes,
    std::function<int(int, size_t)> get_distance
) {
    size_t closest = 0;
    int closest_distance = 0;

    bool checked_any = false;

    for (size_t i : indexes) {
        int distance = get_distance(x, i);

        if (!checked_any || distance < closest_distance) {
            closest = i;
            closest_distance = distance;

            checked_any = true;
        }
    }

    return closest;
}


/* Runs a game of Klondike.
 */
void play_game() {
    // create shuffled deck of cards
    std::vector<Card> deck_vector;

    for (int suit_index = 0; suit_index < 4; ++suit_index) {
        Suit suit;

        switch (suit_index) {
            case 0: suit = CLUBS; break;
            case 1: suit = DIAMONDS; break;
            case 2: suit = HEARTS; break;
            case 3: suit = SPADES; break;
        }

        for (int card_index = 0; card_index < 13; ++card_index) {
            deck_vector.push_back(Card(card_index, suit));
        }
    }

    std::srand(std::time(NULL));
    std::random_shuffle(deck_vector.begin(), deck_vector.end());

    // after dealing, the remainder of the deck is called the "stock", so it's
    // named that here for later even though we haven't dealt yet
    std::stack<Card, std::vector<Card>> stock(deck_vector);

    // cards taken from the stock
    std::vector<Card> taken;

    // history of cards taken from the stock, so the stock can be reset if
    // necessary
    std::stack<Card> stock_history;

    // deal cards
    CardStack tableau[7];

    for (size_t i = 0; i < 7; ++i) {
        // card is unsigned to avoid compiler warning
        for (unsigned int card = 1; card <= i + 1; ++card) {
            if (card == i + 1) {
                tableau[i].cards.push_back(DealtCard(stock.top(), true));
            } else {
                tableau[i].cards.push_back(DealtCard(stock.top(), false));
            }

            stock.pop();
        }
    }

    // create foundations
    Foundation foundations[4] = {
        Foundation(),
        Foundation(),
        Foundation(),
        Foundation()
    };
    
    // create bounding boxes
    // (some fields aren't set here since they are changed each frame)
    wrapper::BBox stock_bbox;

    stock_bbox.x2 = 85;
    stock_bbox.y2 = 110;

    wrapper::BBox top_card_bbox;

    wrapper::BBox foundation_bboxes[4];

    for (size_t i = 0; i < 4; ++i) {
        foundation_bboxes[i].x1 = 273 + 86 * i;
        foundation_bboxes[i].y1 = 15;
        foundation_bboxes[i].x2 = foundation_bboxes[i].x1 + 70;
        foundation_bboxes[i].y2 = 110;
    }

    // type of card(s) being dragged
    DragType drag_type = NONE;

    // cards being dragged
    std::vector<DealtCard> dragged_cards;

    // distances from the top-left corner of the card when dragging started
    int drag_offset_x = 0;
    int drag_offset_y = 0;

    // whether the game has been won
    bool won = false;

    // mainloop
    for (bool closed = false; !closed; closed = wrapper::update()) {
        // get mouse pos
        int mouse_x = wrapper::get_mouse_x();
        int mouse_y = wrapper::get_mouse_y();

        /* game logic */

        if (wrapper::mouse_clicked()) {
            // taking cards off the stock
            if (stock_bbox.collision(mouse_x, mouse_y)) {
                // remove existing taken cards as long as there are still cards
                // in the stock or stock history
                if (!(stock.empty() && stock_history.empty())) {
                    taken.clear();
                }

                if (stock.empty()) {
                    // if the stock is empty, reset it
                    while (!stock_history.empty()) {
                        stock.push(stock_history.top());
                        stock_history.pop();
                    }
                } else {
                    // take 3 cards from the stock if possible, otherwise take
                    // the remainder
                    int i;

                    for (i = 0; i < 3 && !stock.empty(); ++i) {
                        taken.push_back(stock.top());
                        stock_history.push(stock.top());

                        stock.pop();
                    }

                    // update the top card's bounding box
                    top_card_bbox.x1 = 101 + 15 * (i - 1);
                    top_card_bbox.y1 = 15 + 2 * (i - 1);
                    top_card_bbox.x2 = top_card_bbox.x1 + 70;
                    top_card_bbox.y2 = top_card_bbox.y1 + 95;
                }
            }

            // dragging the top card
            if (!taken.empty() && top_card_bbox.collision(mouse_x, mouse_y)) {
                // set drag type and dragged cards
                drag_type = TOP_CARD;
                dragged_cards = {DealtCard(taken.back(), true)};

                // set drag offsets
                drag_offset_x = mouse_x - top_card_bbox.x1;
                drag_offset_y = mouse_y - top_card_bbox.y1;
            }

            // dragging cards from a foundation
            for (size_t i = 0; i < 4; ++i) {
                if (
                    foundation_bboxes[i].collision(mouse_x, mouse_y)
                    && foundations[i].next != 0
                ) {
                    // set drag type
                    switch (i) {
                        case 0: drag_type = FOUNDATION_1; break;
                        case 1: drag_type = FOUNDATION_2; break;
                        case 2: drag_type = FOUNDATION_3; break;
                        case 3: drag_type = FOUNDATION_4; break;
                    }

                    // set dragged cards
                    dragged_cards = {DealtCard(Card(
                        foundations[i].next - 1,
                        foundations[i].suit
                    ), true)};

                    // set drag offsets
                    drag_offset_x = mouse_x - foundation_bboxes[i].x1;
                    drag_offset_y = mouse_y - foundation_bboxes[i].y1;
                }
            }

            // tableau interactions
            for (size_t i = 0; i < 7; ++i) {
                // flip the top card if it was clicked and is face-down
                if (
                    tableau[i].get_top_card_bbox(15 + 86 * i, 126)
                              .collision(mouse_x, mouse_y)
                    && !tableau[i].cards.back().face_up
                ) {
                    tableau[i].cards.back().face_up = true;
                } else {
                    // dragging cards from the tableau

                    // find indexes of the first face-up card and the card
                    // clicked on (which is the lowest card in the stack being
                    // dragged)
                    size_t first_face_up = 0;
                    bool first_face_up_found = false;

                    size_t clicked_card = 0;
                    bool clicked_card_found = false;

                    wrapper::BBox bbox;

                    bbox.x1 = 15 + 86 * i;
                    bbox.y1 = 126;
                    bbox.x2 = 85 + 86 * i;

                    for (size_t j = 0; j < tableau[i].cards.size(); ++j) {
                        if (
                            !first_face_up_found && tableau[i].cards[j].face_up
                        ) {
                            first_face_up = j;
                            first_face_up_found = true;
                        }

                        if (j == tableau[i].cards.size() - 1) {
                            bbox.y2 = bbox.y1 + 95;
                        } else if (tableau[i].cards[j].face_up) {
                            bbox.y2 = bbox.y1 + 14;
                        } else {
                            bbox.y2 = bbox.y1 + 2;
                        }

                        if (
                            !clicked_card_found && first_face_up_found
                            && bbox.collision(mouse_x, mouse_y)
                        ) {
                            clicked_card = j;
                            clicked_card_found = true;
                        }

                        if (tableau[i].cards[j].face_up) {
                            bbox.y1 += 15;
                        } else {
                            bbox.y1 += 3;
                        }
                    }

                    // check if any card was clicked on (a.k.a if this tableau
                    // is being dragged from)
                    if (clicked_card_found) {
                        // set drag type
                        switch (i) {
                            case 0: drag_type = TABLEAU_1; break;
                            case 1: drag_type = TABLEAU_2; break;
                            case 2: drag_type = TABLEAU_3; break;
                            case 3: drag_type = TABLEAU_4; break;
                            case 4: drag_type = TABLEAU_5; break;
                            case 5: drag_type = TABLEAU_6; break;
                            case 6: drag_type = TABLEAU_7; break;
                        }

                        // set dragged cards
                        for (
                            size_t j = clicked_card;
                            j < tableau[i].cards.size();
                            ++j
                        ) {
                            dragged_cards.push_back(tableau[i].cards[j]);
                        }

                        // set drag offsets
                        drag_offset_x = mouse_x - 15 - 86 * i;
                        drag_offset_y = mouse_y - 126 - 3 * first_face_up
                                        - 15 * (clicked_card - first_face_up);
                    }
                }
            }
        }

        // if mouse button is released while dragging, stop dragging and perform
        // release action
        if (drag_type != NONE && !wrapper::mouse_down()) {
            // create bounding box for dragged cards
            wrapper::BBox dragged_cards_bbox(
                mouse_x - drag_offset_x, mouse_y - drag_offset_y,
                mouse_x - drag_offset_x + 70, mouse_y - drag_offset_y
                                              + 15 * dragged_cards.size() + 80
            );

            // whether the move was valid/successful
            bool valid = false;

            // dragging onto foundations
            // (only one card can be dragged here at once)
            if (dragged_cards.size() == 1) {
                // find all foundations the dragged cards are colliding with
                std::vector<size_t> colliding_foundations;

                for (size_t i = 0; i < 4; ++i) {
                    if (dragged_cards_bbox.collision(foundation_bboxes[i])) {
                        colliding_foundations.push_back(i);
                    }
                }

                if (!colliding_foundations.empty()) {
                    // get foundation closest to mouse cursor
                    size_t closest = closest_index(
                        mouse_x - drag_offset_x + 35, colliding_foundations,
                        [](int x, size_t i) -> int {
                            return std::abs(
                                static_cast<int>(x - (308 + 86 * i))
                            );
                        }
                    );

                    // check if the move is valid, performing it if so
                    valid = foundations[closest].add(dragged_cards[0].card);
                }
            }

            // dragging onto tableau
            if (!valid) {
                // find all stacks the dragged cards are colliding with
                std::vector<size_t> colliding_stacks;

                for (size_t i = 0; i < 7; ++i) {
                    if (dragged_cards_bbox.collision(
                        tableau[i].get_top_card_bbox(15 + 86 * i, 126)
                    )) {
                        colliding_stacks.push_back(i);
                    }
                }

                if (!colliding_stacks.empty()) {
                    // get stack closest to mouse cursor
                    size_t closest = closest_index(
                        mouse_x - drag_offset_x + 35, colliding_stacks,
                        [](int x, size_t i) -> int {
                            return std::abs(
                                static_cast<int>(x - (50 + 86 * i))
                            );
                        }
                    );

                    // check if the move is valid, performing it if so
                    Suit tableau_suit = tableau[closest].cards.back().card.suit;
                    Suit dragged_suit = dragged_cards[0].card.suit;

                    if (
                        tableau[closest].cards.back().face_up && (
                            // only kings can be at the bottom of a stack
                            (
                                tableau[closest].cards.empty()
                                && dragged_cards[0].card.index == 12
                            )

                            // card must be one less and the opposite color than
                            // the one below it in the stack
                            || (
                                (
                                    dragged_cards[0].card.index + 1
                                    == tableau[closest].cards.back().card.index
                                ) && (
                                    (
                                        (
                                            dragged_suit == DIAMONDS
                                            || dragged_suit == HEARTS
                                        ) && (
                                            tableau_suit == CLUBS
                                            || tableau_suit == SPADES
                                        )
                                    ) || (
                                        (
                                            dragged_suit == CLUBS
                                            || dragged_suit == SPADES
                                        ) && (
                                            tableau_suit == DIAMONDS
                                            || tableau_suit == HEARTS
                                        )
                                    )
                                )
                            )
                        )
                    ) {
                        for (const DealtCard &card : dragged_cards) {
                            tableau[closest].cards.push_back(
                                DealtCard(card.card, true)
                            );
                        }

                        valid = true;
                    }
                }
            }

            // remove dragged cards from their origin if they were used (i.e.
            // the move was valid)
            if (valid) {
                size_t origin_foundation = 4;
                size_t origin_stack = 7;

                switch (drag_type) {
                    // remove from taken cards (and stock history, so it doesn't
                    // appear in the stock anymore)
                    case TOP_CARD:
                        taken.pop_back();
                        stock_history.pop();
                    break;

                    case FOUNDATION_1: origin_foundation = 0; break;
                    case FOUNDATION_2: origin_foundation = 1; break;
                    case FOUNDATION_3: origin_foundation = 2; break;
                    case FOUNDATION_4: origin_foundation = 3; break;

                    case TABLEAU_1: origin_stack = 0; break;
                    case TABLEAU_2: origin_stack = 1; break;
                    case TABLEAU_3: origin_stack = 2; break;
                    case TABLEAU_4: origin_stack = 3; break;
                    case TABLEAU_5: origin_stack = 4; break;
                    case TABLEAU_6: origin_stack = 5; break;
                    case TABLEAU_7: origin_stack = 6; break;

                    // required to avoid compiler warning
                    default: break;
                }

                // remove from foundation
                if (origin_foundation != 4) {
                    // can only drag one card from a foundation at once
                    --foundations[origin_foundation].next;
                }

                // remove from tableau stack
                if (origin_stack != 7) {
                    for (size_t i = 0; i < dragged_cards.size(); ++i) {
                        tableau[origin_stack].cards.pop_back();
                    }
                }
            }

            // reset drag values; the offsets don't need to be reset
            drag_type = NONE;
            dragged_cards.clear();
        }

        /* drawing */

        // fill screen with the background color from Microsoft Solitaire
        wrapper::clear(wrapper::Color(0, 128, 0));

        // draw stock
        if (stock.empty()) {
            cards_base.draw(15, 15);
        } else {
            int extra_cards = stock.size() / 10;

            for (int i = 0; i <= extra_cards; ++i) {
                cards_back.draw(15 - 2 * i, 15 - 2 * i);
            }

            // update the stock's bounding box
            stock_bbox.x1 = 15 - 2 * extra_cards;
            stock_bbox.y1 = stock_bbox.x1;
        }

        // draw taken cards
        for (
            size_t i = 0;
            i < taken.size() - static_cast<int>(drag_type == TOP_CARD);
            ++i
        ) {
            DealtCard(taken[i], true).draw(101 + 15 * i, 15 + 2 * i);
        }

        // draw foundations
        for (size_t i = 0; i < 4; ++i) {
            // get whether this foundation is being dragged from
            bool dragging_foundation;

            switch (drag_type) {
                case FOUNDATION_1: dragging_foundation = (i == 0); break;
                case FOUNDATION_2: dragging_foundation = (i == 1); break;
                case FOUNDATION_3: dragging_foundation = (i == 2); break;
                case FOUNDATION_4: dragging_foundation = (i == 3); break;

                default: dragging_foundation = false; break;
            }

            foundations[i].draw(273 + 86 * i, 15, dragging_foundation);
        }

        // draw tableau
        for (size_t i = 0; i < 7; ++i) {
            // get whether this stack is being dragged from
            bool dragging_stack;

            switch (drag_type) {
                case TABLEAU_1: dragging_stack = (i == 0); break;
                case TABLEAU_2: dragging_stack = (i == 1); break;
                case TABLEAU_3: dragging_stack = (i == 2); break;
                case TABLEAU_4: dragging_stack = (i == 3); break;
                case TABLEAU_5: dragging_stack = (i == 4); break;
                case TABLEAU_6: dragging_stack = (i == 5); break;
                case TABLEAU_7: dragging_stack = (i == 6); break;

                default: dragging_stack = false; break;
            }

            // draw stack, skipping cards if being dragged from
            tableau[i].draw(
                15 + 86 * i, 126,
                dragging_stack ? dragged_cards.size() : 0
            );
        }

        // draw any cards being dragged
        if (drag_type != NONE) {
            for (size_t i = 0; i < dragged_cards.size(); ++i) {
                dragged_cards[i].draw(
                    mouse_x - drag_offset_x,
                    mouse_y - drag_offset_y + 15 * i
                );
            }
        }

        // exit loop if game has been won (i.e all foundations have 13 cards)
        bool just_won = true;

        for (const Foundation &foundation : foundations) {
            // next could also be called "height", but this is the only place
            // where that name would really make more sense
            if (foundation.next != 13) {
                just_won = false;
                break;
            }
        }

        if (just_won) {
            won = true;
            break;
        }
    }

    // after the game has been won, display the "You won!" text until
    // (being in a separate mainloop means all game logic is disabled and the
    // screen will no longer refresh)
    if (won) {
        wrapper::Sprite("assets/you_win.bmp").draw(0, 0);

        while (!wrapper::update()) {
            continue;
        }
    }
}


int main(int argc, char *argv[]) {
    // initialize the game, using a specific refresh rate if provided;
    // otherwise, use 60 FPS, since it's the most common
    // (There's no cross-platform way to determine refresh rate, so using a
    // command-line argument is the best we can do. This shouldn't cause any
    // problems since the game isn't framerate-dependent.)
    bool success;

    success = wrapper::initialize(
        617, 417, (argc == 2) ? std::atoi(argv[1]) : 60,
        "klondike", "icon.bmp"
    );

    if (!success) {
        return 1;
    }

    // load card sprites
    // (This is in another file to save space. Is this good practice? Probably
    // not... but honestly, I'd rather do this than have 55 lines of *very*
    // repetitive code sitting here.)
    #include "load_cards.cpp"

    // play Klondike game
    play_game();

    // quit wrapper
    wrapper::quit();

    return 0;
}
